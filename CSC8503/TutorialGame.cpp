#include "TutorialGame.h"
#include "GameWorld.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "TextureLoader.h"

#include "PositionConstraint.h"
#include "OrientationConstraint.h"
#include "StateGameObject.h"

#include "NavigationGrid.h"

using namespace NCL;
using namespace CSC8503;

TutorialGame::TutorialGame() : controller(*Window::GetWindow()->GetKeyboard(), *Window::GetWindow()->GetMouse()) {
	world		= new GameWorld();
#ifdef USEVULKAN
	renderer	= new GameTechVulkanRenderer(*world);
	renderer->Init();
	renderer->InitStructures();
#else 
	renderer = new GameTechRenderer(*world);
#endif

	physics		= new PhysicsSystem(*world);

	forceMagnitude	= 10.0f;
	useGravity		= false;
	inSelectionMode = false;

	inGame = true;

	world->GetMainCamera().SetController(controller);

	controller.MapAxis(0, "Sidestep");
	controller.MapAxis(1, "UpDown");
	controller.MapAxis(2, "Forward");

	controller.MapAxis(3, "XLook");
	controller.MapAxis(4, "YLook");

	InitialiseAssets();
}

/*

Each of the little demo scenarios used in the game uses the same 2 meshes, 
and the same texture and shader. There's no need to ever load in anything else
for this module, even in the coursework, but you can add it if you like!

*/
void TutorialGame::InitialiseAssets() {
	cubeMesh	= renderer->LoadMesh("cube.msh");
	sphereMesh	= renderer->LoadMesh("sphere.msh");
	catMesh		= renderer->LoadMesh("ORIGAMI_Chat.msh");
	kittenMesh	= renderer->LoadMesh("Kitten.msh");

	enemyMesh	= renderer->LoadMesh("Keeper.msh");
	bonusMesh	= renderer->LoadMesh("19463_Kitten_Head_v1.msh");
	capsuleMesh = renderer->LoadMesh("capsule.msh");

	basicTex	= renderer->LoadTexture("checkerboard.png");
	basicShader = renderer->LoadShader("scene.vert", "scene.frag");

	InitCamera();
	InitWorld();
}

TutorialGame::~TutorialGame()	{
	delete cubeMesh;
	delete sphereMesh;
	delete catMesh;
	delete kittenMesh;
	delete enemyMesh;
	delete bonusMesh;

	delete basicTex;
	delete basicShader;

	delete physics;
	delete renderer;
	delete world;
}

void TutorialGame::UpdateGame(float dt) {
	if (!inSelectionMode) {
		world->GetMainCamera().UpdateCamera(dt);
	}
	/*
	if (lockedObject != nullptr) {
		Vector3 objPos = lockedObject->GetTransform().GetPosition();
		Vector3 camPos = objPos + lockedOffset;

		Matrix4 temp = Matrix::View(camPos, objPos, Vector3(0,1,0));

		Matrix4 modelMat = Matrix::Inverse(temp);

		Quaternion q(modelMat);
		Vector3 angles = q.ToEuler(); //nearly there now!

		world->GetMainCamera().SetPosition(camPos);
		world->GetMainCamera().SetPitch(angles.x);
		world->GetMainCamera().SetYaw(angles.y);
	}
	*/

	if (inGame) {
		gameOver = false;
		Vector3 playerPos = playerChar->GetTransform().GetPosition();
		Vector3 camOffset = Vector3(0, 50, 0);
		Vector3 camPos = playerPos + camOffset;

		world->GetMainCamera().SetPosition(camPos);

		Vector3 moveDir = Vector3(0, 0, 0);
		Vector3 forward = Vector3(0, 1, 0);
		Quaternion orientation = Quaternion::AxisAngleToQuaterion(forward, 0);

		if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) {
			moveDir += Vector3(0, 0, 1);
			orientation = Quaternion::AxisAngleToQuaterion(forward, 0);
		}
		if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) {
			moveDir += Vector3(0, 0, -1);
			orientation = Quaternion::AxisAngleToQuaterion(forward, 180);
		}
		if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) {
			moveDir += Vector3(1, 0, 0);
			orientation = Quaternion::AxisAngleToQuaterion(forward, 90);
		}
		if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) {
			moveDir += Vector3(-1, 0, 0);
			orientation = Quaternion::AxisAngleToQuaterion(forward, 270);
		}

		playerChar->GetTransform().SetOrientation(orientation);
		playerChar->GetPhysicsObject()->AddForce(moveDir * moveSpeed);

		timer -= dt;
	}
	else {
		if (!gameOver) {
			Window::GetWindow()->ShowOSPointer(true);
			Window::GetWindow()->LockMouseToWindow(false);
			gameOver = true;
		}

		if (gameOver) {
			Debug::Print("Game Over!", Vector2(25, 20));
			Debug::Print("Final Score: " + std::to_string(score), Vector2(20, 25));
			Debug::Print("Press R to try again, or K to quit", Vector2(15, 30));

			if (Window::GetKeyboard()->KeyPressed(KeyCodes::R)) {
				InitWorld();
				inGame = true;
				timer = 60.0f;
				score = 0;
				rescued = 0;
			}
			if (Window::GetKeyboard()->KeyPressed(KeyCodes::K)) {
				Window::DestroyGameWindow();
			}
		}
	}

	Debug::Print("Time Remaining: " + std::to_string(timer), Vector2(50, 5));
	if (timer <= 0.0f || Window::GetKeyboard()->KeyPressed(KeyCodes::J)) {
		inGame = false;
		timer = 0.0f;
	}

	Debug::Print("Score: " + std::to_string(score), Vector2(5, 5));
	Debug::Print("Kittens Rescued: " + std::to_string(rescued), Vector2(5, 10));

	CatCollision();
	EnemyCollision();
	CatRace(dt);

	if (!enemies.empty()) {
		for (StateGameObject* enemy : enemies) {
			if (enemy) {
				enemy->Update(dt);
			}
		}
	}

	UpdateKeys();

	if (useGravity) {
		Debug::Print("(G)ravity on", Vector2(5, 95), Debug::RED);
	}
	else {
		Debug::Print("(G)ravity off", Vector2(5, 95), Debug::RED);
	}
	//This year we can draw debug textures as well!
	//Debug::DrawTex(*basicTex, Vector2(10, 10), Vector2(5, 5), Debug::MAGENTA);

	RayCollision closestCollision;
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::K) && selectionObject) {
		Vector3 rayPos;
		Vector3 rayDir;

		rayDir = selectionObject->GetTransform().GetOrientation() * Vector3(0, 0, -1);

		rayPos = selectionObject->GetTransform().GetPosition();

		Ray r = Ray(rayPos, rayDir);

		if (world->Raycast(r, closestCollision, true, selectionObject)) {
			if (objClosest) {
				objClosest->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
			}
			objClosest = (GameObject*)closestCollision.node;

			objClosest->GetRenderObject()->SetColour(Vector4(1, 0, 1, 1));
		}
	}

	//Debug::DrawLine(Vector3(), Vector3(0, 100, 0), Vector4(1, 0, 0, 1));

	SelectObject();
	MoveSelectedObject();

	world->UpdateWorld(dt);
	renderer->Update(dt);
	physics->Update(dt);

	renderer->Render();
	Debug::UpdateRenderables(dt);
}

void TutorialGame::UpdateKeys() {
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
		InitWorld(); //We can reset the simulation at any time with F1
		selectionObject = nullptr;
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F2)) {
		InitCamera(); //F2 will reset the camera to a specific default place
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::G)) {
		useGravity = !useGravity; //Toggle gravity!
		physics->UseGravity(useGravity);
	}
	//Running certain physics updates in a consistent order might cause some
	//bias in the calculations - the same objects might keep 'winning' the constraint
	//allowing the other one to stretch too much etc. Shuffling the order so that it
	//is random every frame can help reduce such bias.
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F9)) {
		world->ShuffleConstraints(true);
	}
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F10)) {
		world->ShuffleConstraints(false);
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F7)) {
		world->ShuffleObjects(true);
	}
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F8)) {
		world->ShuffleObjects(false);
	}

	if (lockedObject) {
		LockedObjectMovement();
	}
	else {
		DebugObjectMovement();
	}
}

void TutorialGame::LockedObjectMovement() {
	Matrix4 view		= world->GetMainCamera().BuildViewMatrix();
	Matrix4 camWorld	= Matrix::Inverse(view);

	Vector3 rightAxis = Vector3(camWorld.GetColumn(0)); //view is inverse of model!

	//forward is more tricky -  camera forward is 'into' the screen...
	//so we can take a guess, and use the cross of straight up, and
	//the right axis, to hopefully get a vector that's good enough!

	Vector3 fwdAxis = Vector::Cross(Vector3(0, 1, 0), rightAxis);
	fwdAxis.y = 0.0f;
	fwdAxis = Vector::Normalise(fwdAxis);

	if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
		selectionObject->GetPhysicsObject()->AddForce(fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
		selectionObject->GetPhysicsObject()->AddForce(-fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::NEXT)) {
		selectionObject->GetPhysicsObject()->AddForce(Vector3(0,-10,0));
	}
}

void TutorialGame::DebugObjectMovement() {
//If we've selected an object, we can manipulate it with some key presses
	if (inSelectionMode && selectionObject) {
		//Twist the selected object!
		if (Window::GetKeyboard()->KeyDown(KeyCodes::LEFT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(-10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM7)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, 10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM8)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, -10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, -10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, 10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM5)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, -10, 0));
		}
	}
}

void TutorialGame::InitCamera() {
	world->GetMainCamera().SetNearPlane(0.1f);
	world->GetMainCamera().SetFarPlane(500.0f);
	world->GetMainCamera().SetPitch(-90.0f);
	world->GetMainCamera().SetYaw(180.0f);
	world->GetMainCamera().SetPosition(Vector3(0, 100, 0));
	lockedObject = nullptr;
}

void TutorialGame::InitWorld() {
	world->ClearAndErase();
	physics->Clear();

	//InitMixedGridWorld(15, 15, 3.5f, 3.5f);//Default
	//InitCubeGridWorld(15, 15, 3.5f, 3.5f, Vector3(1, 1, 1));
	//BridgeConstraintTest();//Constraint Tutorial

	InitDefaultFloor();//Default

	InitEnvironment();
	InitPlayer();
	InitKittens();
	InitCoins();
	InitEnemies();
	InitSphereGridWorld(9, 6, 4, 4, 3, Vector3(90, 0, 90));
	InitSphereGridWorld(9, 6, 4, 4, 3, Vector3(95, 5, 90));
	InitButton();
	InitRacer();
}

/*

A single function to add a large immoveable cube to the bottom of our world

*/
GameObject* TutorialGame::AddFloorToWorld(const Vector3& position) {
	GameObject* floor = new GameObject();

	Vector3 floorSize = Vector3(600, 2, 600);
	AABBVolume* volume = new AABBVolume(floorSize);
	floor->SetBoundingVolume((CollisionVolume*)volume);
	floor->GetTransform()
		.SetScale(floorSize * 2.0f)
		.SetPosition(position);

	floor->SetRenderObject(new RenderObject(&floor->GetTransform(), cubeMesh, basicTex, basicShader));
	floor->SetPhysicsObject(new PhysicsObject(&floor->GetTransform(), floor->GetBoundingVolume()));

	floor->GetPhysicsObject()->SetInverseMass(0);
	floor->GetPhysicsObject()->InitCubeInertia();

	world->AddGameObject(floor);

	return floor;
}

GameObject* TutorialGame::AddWallToWorld(const Vector3& position, float width, float height) {
	GameObject* wall = new GameObject();

	Vector3 wallSize = Vector3(width * 0.5f, 10.0f, height * 0.5f);
	AABBVolume* volume = new AABBVolume(wallSize);
	wall->SetBoundingVolume((CollisionVolume*)volume);
	wall->GetTransform().SetScale(wallSize * 2.0f).SetPosition(position);

	wall->SetRenderObject(new RenderObject(&wall->GetTransform(), cubeMesh, basicTex, basicShader));
	wall->SetPhysicsObject(new PhysicsObject(&wall->GetTransform(), wall->GetBoundingVolume()));

	wall->GetPhysicsObject()->SetInverseMass(0);
	wall->GetPhysicsObject()->InitCubeInertia();

	world->AddGameObject(wall);

	return wall;
}

/*

Builds a game object that uses a sphere mesh for its graphics, and a bounding sphere for its
rigid body representation. This and the cube function will let you build a lot of 'simple' 
physics worlds. You'll probably need another function for the creation of OBB cubes too.

*/
GameObject* TutorialGame::AddSphereToWorld(const Vector3& position, float radius, float inverseMass) {
	GameObject* sphere = new GameObject();

	Vector3 sphereSize = Vector3(radius, radius, radius);
	SphereVolume* volume = new SphereVolume(radius);
	sphere->SetBoundingVolume((CollisionVolume*)volume);

	sphere->GetTransform()
		.SetScale(sphereSize)
		.SetPosition(position);

	sphere->SetRenderObject(new RenderObject(&sphere->GetTransform(), sphereMesh, basicTex, basicShader));
	sphere->SetPhysicsObject(new PhysicsObject(&sphere->GetTransform(), sphere->GetBoundingVolume()));

	sphere->GetPhysicsObject()->SetInverseMass(inverseMass);
	sphere->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(sphere);

	return sphere;
}

GameObject* TutorialGame::AddCubeToWorld(const Vector3& position, Vector3 dimensions, float inverseMass) {
	GameObject* cube = new GameObject();

	AABBVolume* volume = new AABBVolume(dimensions);
	cube->SetBoundingVolume((CollisionVolume*)volume);

	cube->GetTransform()
		.SetPosition(position)
		.SetScale(dimensions * 2.0f);

	cube->SetRenderObject(new RenderObject(&cube->GetTransform(), cubeMesh, basicTex, basicShader));
	cube->SetPhysicsObject(new PhysicsObject(&cube->GetTransform(), cube->GetBoundingVolume()));

	cube->GetPhysicsObject()->SetInverseMass(inverseMass);
	cube->GetPhysicsObject()->InitCubeInertia();

	world->AddGameObject(cube);

	return cube;
}

GameObject* TutorialGame::AddPlayerToWorld(const Vector3& position) {
	float meshSize		= 15.0f;
	float inverseMass	= 0.5f;

	GameObject* character = new GameObject();
	SphereVolume* volume  = new SphereVolume(1.0f);

	character->SetBoundingVolume((CollisionVolume*)volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(&character->GetTransform(), catMesh, nullptr, basicShader));
	character->SetPhysicsObject(new PhysicsObject(&character->GetTransform(), character->GetBoundingVolume()));

	character->GetRenderObject()->SetColour(Vector4(0, 1, 1, 1));

	character->GetPhysicsObject()->SetInverseMass(inverseMass);
	character->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(character);

	return character;
}

GameObject* TutorialGame::AddKittenToWorld(const Vector3& position) {
	float meshSize = 10.0f;
	float inverseMass = 0.5f;
	GameObject* kitten = new GameObject();
	SphereVolume* volume = new SphereVolume(1.0f);

	kitten->SetBoundingVolume((CollisionVolume*)volume);
	kitten->GetTransform().SetScale(Vector3(meshSize, meshSize, meshSize)).SetPosition(position);

	kitten->SetRenderObject(new RenderObject(&kitten->GetTransform(), catMesh, nullptr, basicShader));
	kitten->SetPhysicsObject(new PhysicsObject(&kitten->GetTransform(), kitten->GetBoundingVolume()));

	kitten->GetRenderObject()->SetColour(Vector4(0, 0, 1, 1));

	kitten->GetPhysicsObject()->SetInverseMass(inverseMass);
	kitten->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(kitten);

	gotKitten = kitten;

	return kitten;
}

GameObject* TutorialGame::AddEnemyToWorld(const Vector3& position) {
	float meshSize		= 10.0f;
	float inverseMass	= 0.5f;

	GameObject* character = new GameObject();

	AABBVolume* volume = new AABBVolume(Vector3(0.3f, 0.9f, 0.3f) * meshSize);
	character->SetBoundingVolume((CollisionVolume*)volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(&character->GetTransform(), enemyMesh, nullptr, basicShader));
	character->SetPhysicsObject(new PhysicsObject(&character->GetTransform(), character->GetBoundingVolume()));

	character->GetPhysicsObject()->SetInverseMass(inverseMass);
	character->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(character);

	return character;
}

StateGameObject* TutorialGame::AddStateObjectToWorld(const Vector3& position) {
	float meshSize = 10.0f;
	float inverseMass = 0.5f;

	StateGameObject* character = new StateGameObject();

	AABBVolume* volume = new AABBVolume(Vector3(0.3f, 0.9f, 0.3f) * meshSize);
	character->SetBoundingVolume((CollisionVolume*)volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(&character->GetTransform(), enemyMesh, nullptr, basicShader));
	character->SetPhysicsObject(new PhysicsObject(&character->GetTransform(), character->GetBoundingVolume()));

	character->GetRenderObject()->SetColour(Vector4(1, 0.5, 0, 1));

	character->GetPhysicsObject()->SetInverseMass(inverseMass);
	character->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(character);

	return character;
}

GameObject* TutorialGame::AddBonusToWorld(const Vector3& position) {
	GameObject* apple = new GameObject();

	SphereVolume* volume = new SphereVolume(0.5f);
	apple->SetBoundingVolume((CollisionVolume*)volume);
	apple->GetTransform()
		.SetScale(Vector3(6, 6, 6))
		.SetPosition(position);

	apple->SetRenderObject(new RenderObject(&apple->GetTransform(), bonusMesh, nullptr, basicShader));
	apple->SetPhysicsObject(new PhysicsObject(&apple->GetTransform(), apple->GetBoundingVolume()));

	apple->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1));

	apple->GetPhysicsObject()->SetInverseMass(1.0f);
	apple->GetPhysicsObject()->InitSphereInertia();

	world->AddGameObject(apple);

	return apple;
}

void TutorialGame::InitDefaultFloor() {
	AddFloorToWorld(Vector3(0, -12, 0));
}

void TutorialGame::InitPlayer() {
	playerChar = AddPlayerToWorld(Vector3(10, -7, 15));
}

void TutorialGame::InitKittens() {
	kittens.push_back(AddKittenToWorld(Vector3(100, -7, 109)));
	kittens.push_back(AddKittenToWorld(Vector3(400, -7, 15)));
	kittens.push_back(AddKittenToWorld(Vector3(200, -7, 109)));

	kittens.push_back(AddKittenToWorld(Vector3(20, -7, 400)));
	kittens.push_back(AddKittenToWorld(Vector3(230, -7, 432)));
	kittens.push_back(AddKittenToWorld(Vector3(405, -7, 375)));
	kittens.push_back(AddKittenToWorld(Vector3(150, -7, 478)));
	kittens.push_back(AddKittenToWorld(Vector3(435, -7, 440)));
}

void TutorialGame::InitCoins() {
	coins.push_back(AddBonusToWorld(Vector3(100, -7, 15)));
	coins.push_back(AddBonusToWorld(Vector3(150, -7, 15)));
	coins.push_back(AddBonusToWorld(Vector3(200, -7, 15)));
	coins.push_back(AddBonusToWorld(Vector3(250, -7, 15)));

	coins.push_back(AddBonusToWorld(Vector3(400, -7, 109)));
	coins.push_back(AddBonusToWorld(Vector3(350, -7, 109)));
	coins.push_back(AddBonusToWorld(Vector3(300, -7, 109)));
	coins.push_back(AddBonusToWorld(Vector3(250, -7, 109)));

	coins.push_back(AddBonusToWorld(Vector3(450, -7, 270)));
	coins.push_back(AddBonusToWorld(Vector3(450, -7, 240)));
	coins.push_back(AddBonusToWorld(Vector3(450, -7, 300)));
	coins.push_back(AddBonusToWorld(Vector3(450, -7, 330)));
}

void TutorialGame::InitEnemies() {
	enemies.push_back(AddStateObjectToWorld(Vector3(400, 3, 80)));
	enemies.push_back(AddStateObjectToWorld(Vector3(50, 3, 25)));
	enemies.push_back(AddStateObjectToWorld(Vector3(320, 3, 260)));
	enemies.push_back(AddStateObjectToWorld(Vector3(270, 3, 260)));
	enemies.push_back(AddStateObjectToWorld(Vector3(250, 3, 260)));

	enemies.push_back(AddStateObjectToWorld(Vector3(350, 3, 385)));
	enemies.push_back(AddStateObjectToWorld(Vector3(100, 3, 450)));
}

void TutorialGame::InitButton() {
	button = AddCubeToWorld(Vector3(9, -5, 210), Vector3(2, 6, 6), 1.0f);
	button->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1));
	button->GetPhysicsObject()->SetInverseMass(0);
	gate = AddCubeToWorld(Vector3(220, -5, 350), Vector3(50, 10, 5), 1.0f);
	gate->GetRenderObject()->SetColour(Vector4(1, 0, 1, 1));
	gate->GetPhysicsObject()->SetInverseMass(0);
}

void TutorialGame::InitRacer() {
	raceCat = AddPlayerToWorld(Vector3(15, -7, 40));
	raceCat->GetRenderObject()->SetColour(Vector4(0.5, 0.25, 0.25, 1));
	Vector3 forward = Vector3(0, 1, 0);
	Quaternion orientation = Quaternion::AxisAngleToQuaterion(forward, 90);
	raceCat->GetTransform().SetOrientation(orientation);
}

void TutorialGame::InitEnvironment() {
	NavigationGrid grid("GameWorld.txt");
	int gridHeight = grid.GetGridHeight();
	int gridWidth = grid.GetGridWidth();
	GridNode* allNodes = grid.GetAllNodes();

	std::vector<std::vector<bool>> visited(gridHeight, std::vector<bool>(gridWidth, false));

	for (int y = 0; y < gridHeight; ++y) {
		int startX = -1;
		for (int x = 0; x < gridWidth; ++x) {
			GridNode& n = allNodes[(gridWidth * y) + x];

			if (n.type == 'x' && !visited[y][x]) {
				if (startX == -1) {
					startX = x;
				}
				visited[y][x] = true;
			}
			else if (startX != -1) {
				int endX = x;
				AddWallToWorld(Vector3((startX + endX - 1) * 5, 0, y * 10), (endX - startX) * 10, 10);
				startX = -1;
			}
		}
		if (startX != -1) {
			AddWallToWorld(Vector3((startX + gridWidth - 1) * 5, 0, y * 10), (gridWidth - startX) * 10, 10);
		}
	}

	for (int x = 0; x < gridWidth; ++x) {
		int startY = -1;
		for (int y = 0; y < gridHeight; ++y) {
			GridNode& n = allNodes[(gridWidth * y) + x];

			if (n.type == 'x' && !visited[y][x]) {
				if (startY == -1) {
					startY = y;
				}
				visited[y][x] = true;
			}
			else if (startY != -1) {
				int endY = y;
				AddWallToWorld(Vector3(x * 10, 1, (startY + endY - 1) * 5), 10, (endY - startY) * 10);
				startY = -1;
			}
		}
		if (startY != -1) {
			AddWallToWorld(Vector3(x * 10, 1, (startY + gridHeight - 1) * 5), 10, (gridHeight - startY) * 10);
		}
	}
}

void TutorialGame::InitSphereGridWorld(int numRows, int numCols, float rowSpacing, float colSpacing, float radius, Vector3 spawn) {
	for (int x = 0; x < numCols; ++x) {
		for (int z = 0; z < numRows; ++z) {
			Vector3 position = spawn + Vector3(x * colSpacing, 50.0f, z * rowSpacing);
			GameObject* sphere = AddSphereToWorld(position, radius, 1.0f);
			spheres.push_back(sphere);
		}
	}
}

void TutorialGame::InitMixedGridWorld(int numRows, int numCols, float rowSpacing, float colSpacing) {
	float sphereRadius = 1.0f;
	Vector3 cubeDims = Vector3(1, 1, 1);

	for (int x = 0; x < numCols; ++x) {
		for (int z = 0; z < numRows; ++z) {
			Vector3 position = Vector3(x * colSpacing, 10.0f, z * rowSpacing);

			if (rand() % 2) {
				AddCubeToWorld(position, cubeDims);
			}
			else {
				AddSphereToWorld(position, sphereRadius);
			}
		}
	}
}

void TutorialGame::InitCubeGridWorld(int numRows, int numCols, float rowSpacing, float colSpacing, const Vector3& cubeDims) {
	for (int x = 1; x < numCols+1; ++x) {
		for (int z = 1; z < numRows+1; ++z) {
			Vector3 position = Vector3(x * colSpacing, 10.0f, z * rowSpacing);
			AddCubeToWorld(position, cubeDims, 1.0f);
		}
	}
}

void TutorialGame::BridgeConstraintTest() {
	Vector3 cubeSize = Vector3(8, 8, 8);

	float invCubeMass = 5;
	int numLinks = 10;
	float maxDistance = 30;
	float cubeDistance = 20;

	Vector3 startPos = Vector3(500, 500, 500);

	GameObject* start = AddCubeToWorld(startPos + Vector3(0, 0, 0), cubeSize, 0);
	GameObject* end = AddCubeToWorld(startPos + Vector3((numLinks + 2) * cubeDistance, 0, 0), cubeSize, 0);
	GameObject* previous = start;

	for (int i = 0; i < numLinks; ++i) {
		GameObject* block = AddCubeToWorld(startPos + Vector3((i + 1) * cubeDistance, 0, 0), cubeSize, invCubeMass);
		PositionConstraint* constraint = new PositionConstraint(previous, block, maxDistance);
		world->AddConstraint(constraint);
		previous = block;
	}
	PositionConstraint* constraint = new PositionConstraint(previous, end, maxDistance);
	world->AddConstraint(constraint);
}

/*
Every frame, this code will let you perform a raycast, to see if there's an object
underneath the cursor, and if so 'select it' into a pointer, so that it can be 
manipulated later. Pressing Q will let you toggle between this behaviour and instead
letting you move the camera around. 

*/
bool TutorialGame::SelectObject() {
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::Q)) {
		inSelectionMode = !inSelectionMode;
		if (inSelectionMode) {
			Window::GetWindow()->ShowOSPointer(true);
			Window::GetWindow()->LockMouseToWindow(false);
		}
		else {
			Window::GetWindow()->ShowOSPointer(false);
			Window::GetWindow()->LockMouseToWindow(true);
		}
	}
	if (inSelectionMode) {
		Debug::Print("Press Q to change to camera mode!", Vector2(5, 85));

		if (Window::GetMouse()->ButtonDown(NCL::MouseButtons::Left)) {
			if (selectionObject) {	//set colour to deselected;
				selectionObject->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
				selectionObject = nullptr;
			}

			Ray ray = CollisionDetection::BuildRayFromMouse(world->GetMainCamera());

			RayCollision closestCollision;
			if (world->Raycast(ray, closestCollision, true)) {
				selectionObject = (GameObject*)closestCollision.node;

				selectionObject->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1));
				return true;
			}
			else {
				return false;
			}
		}
		if (Window::GetKeyboard()->KeyPressed(NCL::KeyCodes::L)) {
			if (selectionObject) {
				if (lockedObject == selectionObject) {
					lockedObject = nullptr;
				}
				else {
					lockedObject = selectionObject;
				}
			}
		}
	}
	else {
		Debug::Print("Press Q to change to select mode!", Vector2(5, 85));
	}
	return false;
}

/*
If an object has been clicked, it can be pushed with the right mouse button, by an amount
determined by the scroll wheel. In the first tutorial this won't do anything, as we haven't
added linear motion into our physics system. After the second tutorial, objects will move in a straight
line - after the third, they'll be able to twist under torque aswell.
*/

void TutorialGame::MoveSelectedObject() {
	Debug::Print("Click Force:" + std::to_string(forceMagnitude), Vector2(5, 90));
	forceMagnitude += Window::GetMouse()->GetWheelMovement() * 100.0f;

	if (!selectionObject) {
		return;//we haven't selected anything!
	}
	//Push the selected object!
	if (Window::GetMouse()->ButtonPressed(NCL::MouseButtons::Right)) {
		Ray ray = CollisionDetection::BuildRayFromMouse(world->GetMainCamera());

		RayCollision closestCollision;
		if (world->Raycast(ray, closestCollision, true)) {
			if (closestCollision.node == selectionObject) {
				selectionObject->GetPhysicsObject()->AddForceAtPosition(ray.GetDirection() * forceMagnitude, closestCollision.collidedAt);
			}
		}
	}
}

void TutorialGame::CatCollision() {
	CollisionDetection::CollisionInfo info;

	for (auto i = kittens.begin(); i != kittens.end();) {
		GameObject* kitten = *i;

		if (CollisionDetection::ObjectIntersection(playerChar, kitten, info)) {
			kitten->GetTransform().SetPosition(Vector3(10, -50, 15));
			rescued += 1;
			score += 2000;

			i = kittens.erase(i);
		}
		else {
			++i;
		}
	}
	for (auto i = coins.begin(); i != coins.end();) {
		GameObject* coin = *i;

		if (CollisionDetection::ObjectIntersection(playerChar, coin, info)) {
			coin->GetTransform().SetPosition(Vector3(20, -50, 15));
			score += 1000;

			i = coins.erase(i);
		}
		else {
			++i;
		}
	}
	for (auto i = spheres.begin(); i != spheres.end();) {
		GameObject* sphere = *i;

		if (CollisionDetection::ObjectIntersection(button, sphere, info)) {
			gate->GetTransform().SetPosition(Vector3(100, -200, 100));

			i = spheres.erase(i);
		}
		else {
			++i;
		}
	}
}

void TutorialGame::EnemyCollision() {
	CollisionDetection::CollisionInfo info;

	for (auto i = enemies.begin(); i != enemies.end();) {
		StateGameObject* enemy = *i;

		if (CollisionDetection::ObjectIntersection(playerChar, enemy, info)) {
			inGame = false;
		}
		else {
			++i;
		}
	}
}

void TutorialGame::CatRace(float dt) {
	CollisionDetection::CollisionInfo info;
	static std::vector<Vector3>racePath;
	static bool pathInit = false;
	static bool input = false;
	static int waypoint = 0;


	if (CollisionDetection::ObjectIntersection(playerChar, raceCat, info) && !raceFlag) {
		Debug::Print("Hey! Would you like to race? (Y/N)", Vector2(25, 25), Debug::RED);
		raceFlag = true;
		input = true;
	}

	if (input) {
		Debug::Print("Hey! Would you like to race? (Y/N)", Vector2(25, 25), Debug::RED);
		if (Window::GetKeyboard()->KeyPressed(KeyCodes::Y)) {
			NavigationGrid grid("GameWorld.txt");
			NavigationPath outPath;

			Vector3 startPos = raceCat->GetTransform().GetPosition();
			Vector3 endPos(480, -1, 480);

			bool found = grid.FindPath(startPos, endPos, outPath);

			Vector3 pos;
			while (outPath.PopWaypoint(pos)) {
				racePath.push_back(pos);
			}
			pathInit = true;
			input = false;
			return;
		}
		else if (Window::GetKeyboard()->KeyPressed(KeyCodes::N)) {
			playerChar->GetTransform().SetPosition(Vector3(10, -7, 15));
			raceFlag = false;
			input = false;
			return;
		}
		return;
	}

	if (raceFlag && pathInit) {
		if (waypoint < racePath.size()) {
			Vector3 currentPos = raceCat->GetTransform().GetPosition();
			Vector3 targetPos = racePath[waypoint];

			Vector3 direction = (targetPos - currentPos);
			direction = Vector::Normalise(direction);
			float speed = 30.0f;

			raceCat->GetTransform().SetPosition(currentPos + direction * speed * dt);
			raceCat->GetPhysicsObject()->AddTorque(direction);

			if (Vector::Length(targetPos - currentPos) < 1.0f) {
				waypoint++;
			}
		}
		else {
			Debug::Print("Race Complete!", Vector2(25, 25), Debug::YELLOW);
			raceFlag = false;
			pathInit = false;
			racePath.clear();
			waypoint = 0;
		}
	}
}