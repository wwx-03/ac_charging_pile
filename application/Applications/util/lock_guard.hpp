#pragma once

template <typename T>
class LockGuard {
public:
	explicit LockGuard(T *object) : object_(object) {
		object_->Lock();
	}

	~LockGuard() {
		object_->Unlock();
	}
private:
	T *object_;
};
