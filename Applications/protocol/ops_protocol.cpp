#include "ops_protocol.hpp"

OpsProtocol::OpsProtocol(Network *network) : Protocol(network) {

}

void OpsProtocol::ChildStart() {
}

void OpsProtocol::OnDataReceived(const uint8_t* data, size_t length) {
	// 处理接收到的数据，根据协议解析并触发相应事件
	// 这里仅为示例，实际实现需要根据具体协议进行解析
	
}


