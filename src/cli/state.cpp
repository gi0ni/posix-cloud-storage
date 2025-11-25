#include "state.h"

State* State::instance;

State::State()
{
	instance = nullptr;
}

State& State::GetInstance()
{
	if(instance == nullptr)
		instance = new State();
	return *instance;
}

State& glb = State::GetInstance();
