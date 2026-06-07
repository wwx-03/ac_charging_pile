#include "billing/billing_engine.hpp"

#include <string.h>
#include <time.h>

#include <log/log>
#include <custom>

#include "config.hpp"

BillingEngine::BillingEngine(size_t channel, Charger *charger, Meter *meter, Storage *storage)
		: channel_(channel)
		, charger_(charger)
		, meter_(meter)
		, storage_(storage) {
	timer_ = xTimerCreate(nullptr, pdMS_TO_TICKS(1000), pdTRUE, this, +[](TimerHandle_t timer) {
		auto *engine = static_cast<BillingEngine *>(pvTimerGetTimerID(timer));
		engine->UpdateConsumption();
	});
	configASSERT(timer_);
}

BillingEngine::~BillingEngine() {
	xTimerStop(timer_, pdMS_TO_TICKS(100));
	xTimerDelete(timer_, pdMS_TO_TICKS(100));
}

void BillingEngine::StartSession(const uint8_t *transaction_sn, 
				 size_t sn_len, 
				 const uint8_t *logic_card_id, 
				 size_t logic_card_id_len, 
				 const uint8_t *physic_card_id, 
				 size_t physic_card_id_len,
				 float balance) {
	// 如果已经有会话在进行，先强制结束它（可能是之前异常断电导致的）
	if (session_active_) {
		LOGW("BillingEngine: session already active, force-stopping previous");
		StopSession(StopReason::FAULT);
	}
	
	// 初始化新会话数据
	memset(&current_session_, 0, sizeof(current_session_));

	if (transaction_sn && sn_len > 0) {
		size_t safe_len = custom::min(sn_len, sizeof(current_session_.session_id));
		memcpy(current_session_.session_id, transaction_sn, safe_len);
	}

	if (logic_card_id && logic_card_id_len > 0) {
		size_t safe_len = custom::min(logic_card_id_len, sizeof(current_session_.logic_card_id));
		memcpy(current_session_.logic_card_id, logic_card_id, safe_len);
	}

	if (physic_card_id && physic_card_id_len > 0) {
		size_t safe_len = custom::min(physic_card_id_len, sizeof(current_session_.physic_card_id));
		memcpy(current_session_.physic_card_id, physic_card_id, safe_len);
	}

	// current_session_.start_time	  = time(nullptr);
	current_session_.start_energy_kwh = meter_->GetEnergy(channel_);
	current_session_.balance          = balance;
	current_session_.valid            = true;
	session_active_                   = true;

	// 启动会话后立即保存快照，以便在异常断电时恢复
	SaveSnapshot();
	
	xTimerStart(timer_, pdMS_TO_TICKS(100));
}

ChargingSession BillingEngine::StopSession(StopReason reason) {
	if (!session_active_) {
		LOGW("BillingEngine: StopSession called without active session");
		return ChargingSession{};
	}

	// current_session_.stop_time   = time(nullptr);
	current_session_.stop_reason = reason;
	session_active_              = false;

	// 清除 Flash 中的会话快照（会话已正常结束）
	EraseSnapshot();

	xTimerStop(timer_, pdMS_TO_TICKS(100));

	return current_session_;
}

float BillingEngine::GetCurrentEnergy() const {
	if (!session_active_) {
		return 0.0f;
	}
	float consumed = meter_->GetEnergy(channel_) - current_session_.start_energy_kwh;
	return consumed > 0.0f ? consumed : 0.0f;
}

float BillingEngine::GetCurrentCost() const {
	return current_session_.total_cost;
}

void BillingEngine::SaveSnapshot() {
	// storage_->Save(config::STORED_BILLING_SESSION_ADDR, reinterpret_cast<const uint8_t *>(&current_session_), sizeof(current_session_));
}

bool BillingEngine::RecoverSession() {
#if 0
	ChargingSession saved{};
	storage_->Load(config::STORED_BILLING_SESSION_ADDR,
				   reinterpret_cast<uint8_t *>(&saved), sizeof(saved));

	if (!saved.valid) {
		return false;
	}

	LOGW("BillingEngine: recovered interrupted session from Flash | start_time=%lu",
		 (unsigned long)saved.start_time);

	current_session_ = saved;
	session_active_  = true;
#endif
	return true;
}

void BillingEngine::UpdateConsumption() {
	if (!session_active_) {
		return;
	}
	++current_session_.duration_seconds;
	current_session_.stop_energy_kwh = meter_->GetEnergy(channel_);
	current_session_.consumed_energy_kwh = custom::abs(current_session_.stop_energy_kwh - current_session_.start_energy_kwh);
	if (current_session_.consumed_energy_kwh - current_session_.previous_consumed_energy_kwh > 1.0f) {
		// 电量异常跳变（可能是电表重置），以之前的消耗为准，避免计费金额倒退
		current_session_.start_energy_kwh = current_session_.stop_energy_kwh - current_session_.previous_consumed_energy_kwh;
	}
	current_session_.previous_consumed_energy_kwh = current_session_.consumed_energy_kwh;

	float slot_consumed = current_session_.consumed_energy_kwh - current_session_.previous_consumed_energy_kwh;

	// 根据当前时间计算时段
	struct tm timeinfo{};
	// time_t now = time(nullptr);
	// _localtime_r(&now, &timeinfo);
	uint8_t slot = (timeinfo.tm_hour * 60 + timeinfo.tm_min) / 30;  // 48 个半小时的时段划分
	if (slot >= 48) { slot = 0; }
	uint8_t period = billing_model_.time_slot[slot];
	switch (period) {
		case 0: current_session_.consumed_energy_kwh_sharp += slot_consumed; break;
		case 1: current_session_.consumed_energy_kwh_peak  += slot_consumed; break;
		case 2: current_session_.consumed_energy_kwh_flat  += slot_consumed; break;
		case 3: current_session_.consumed_energy_kwh_valley += slot_consumed; break;
		default: break;
	}
	// 计算费用
	current_session_.cost_sharp_kwh         = current_session_.consumed_energy_kwh_sharp * billing_model_.price_per_kwh_sharp;
	current_session_.service_fee_sharp_kwh  = current_session_.consumed_energy_kwh_sharp * billing_model_.service_fee_per_kwh_sharp;
	current_session_.cost_peak_kwh          = current_session_.consumed_energy_kwh_peak  * billing_model_.price_per_kwh_peak;
	current_session_.service_fee_peak_kwh   = current_session_.consumed_energy_kwh_peak * billing_model_.service_fee_per_kwh_peak;
	current_session_.cost_flat_kwh          = current_session_.consumed_energy_kwh_flat  * billing_model_.price_per_kwh_flat;
	current_session_.service_fee_flat_kwh   = current_session_.consumed_energy_kwh_flat * billing_model_.service_fee_per_kwh_flat;
	current_session_.cost_valley_kwh        = current_session_.consumed_energy_kwh_valley * billing_model_.price_per_kwh_valley;
	current_session_.service_fee_valley_kwh = current_session_.consumed_energy_kwh_valley * billing_model_.service_fee_per_kwh_valley;
	
	current_session_.total_cost  = current_session_.cost_sharp_kwh
	                             + current_session_.service_fee_sharp_kwh
	                             + current_session_.cost_peak_kwh
				     + current_session_.service_fee_peak_kwh
	                             + current_session_.cost_flat_kwh
				     + current_session_.service_fee_flat_kwh
	                             + current_session_.cost_valley_kwh
				     + current_session_.service_fee_valley_kwh;
	
	if (current_session_.total_cost >= current_session_.balance) {
		charger_->SendEvent(Charger::Event::EV_BALANCE_DONE);
	}
}

void BillingEngine::EraseSnapshot() {
	ChargingSession empty{};
	// storage_->Save(config::STORED_BILLING_SESSION_ADDR, reinterpret_cast<const uint8_t *>(&empty), sizeof(empty));
}
