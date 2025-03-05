#include "StateGameObject.h"
#include "StateTransition.h"
#include "StateMachine.h"
#include "State.h"
#include "PhysicsObject.h"
#include "CollisionDetection.h"

using namespace NCL;
using namespace CSC8503;

StateGameObject::StateGameObject() {
	counter = 0.0f;
	basicAI = new StateMachine();

	State* stateA = new State([&](float dt)->void {
		this->MoveUp(dt);
		});

	State* stateB = new State([&](float dt)->void {
		this->MoveDown(dt);
		});

	basicAI->AddState(stateA);
	basicAI->AddState(stateB);

	basicAI->AddTransition(new StateTransition(stateA, stateB, [&]()->bool {
		return this->counter > 3.0f;
		}));

	basicAI->AddTransition(new StateTransition(stateB, stateA, [&]()->bool {
		return this->counter < 0.0f;
		}));
}

StateGameObject::~StateGameObject() {
	delete basicAI;
}

void StateGameObject::Update(float dt) {
	basicAI->Update(dt);
}

void StateGameObject::MoveUp(float dt) {
	GetPhysicsObject()->AddForce({ 0, 0, -20 });
	counter += dt;
}

void StateGameObject::MoveDown(float dt) {
	GetPhysicsObject()->AddForce({ 0, 0, 20 });
	counter -= dt;
}