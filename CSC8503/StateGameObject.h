#pragma once
#include "GameObject.h"

namespace NCL {
    namespace CSC8503 {
        class StateMachine;
        class StateGameObject : public GameObject  {
        public:
            StateGameObject();
            ~StateGameObject();

            virtual void Update(float dt);

        protected:
            void MoveUp(float dt);
            void MoveDown(float dt);

            StateMachine* basicAI;
            float counter;
        };
    }
}
