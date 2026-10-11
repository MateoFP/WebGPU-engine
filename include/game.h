#include "include/gpumain.h"

struct Ray
{
	v3 position;
	v3 direction;
};
struct RayCollision
{
	bool hit;
	float distance;
	v3 point;
	v3 normal;
};
struct ButtonState
{
	uint32_t half_transition_count;
	bool ended_down;
	bool pressed;
};
struct Input
{
	RayCollision collision = {};
	Ray ray = {};
	v2 mouse_pos = {};

	union
	{
		ButtonState buttons[2];
		struct
		{
			ButtonState spacebar;
			ButtonState a;
		};
	};
};
struct GameContext
{
	bool running = true;
	bool focused = true;
};
struct Camera
{
	mat4 model;
	mat4 view;
	mat4 proj;
	v3 pos;
	v3 look_at;
	float speed;
};

extern GameContext gc;
extern Camera cam;
extern Input input;
extern BoundingBox world_box;

v3 RayCast(double x, double y, mat4 view, mat4 projection, float width, float height);
RayCollision GetRayCollisionBox(Ray ray, BoundingBox box);
mat4 TSR(v3 translate, float scale, float angle);
v2 WorldToScreen(v3 world_pos, mat4 view_proj);
void UpdateStatic();
void PrintTrees();
void PrintGrid();
void UpdateVertexGrid();
void AddTree();
void DeleteTree();
void InitWorld();
void UpdatePositions();
void UpdateGPU();
void UpdateEntityAnimation(Entity* entity, float delta);
void SetEntityAnimation(Entity* entity, uint32_t new_anim_id, bool loop);

void UpdateGame(float delta);