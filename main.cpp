#include <iostream>
#include <vector>
//# define GLM_COORDINATE_SYSTEM GLM_RIGHT_HANDED

#include "bigg.hpp"
#include "glm/glm.hpp"
#include "glm/matrix.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/matrix_transform.inl"
#include "bigg/deps/bgfx.cmake/bgfx/examples/common/debugdraw/debugdraw.h"
#include "bigg/deps/bgfx.cmake/bx/include/bx/math.h"

using namespace glm;

bool isShiftDown = false;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (mods == GLFW_MOD_SHIFT)
		isShiftDown = true;
	else
		isShiftDown = false;
}

inline mat3 CreateFromForwardVector(vec3 forward)
{
	mat3 m;
	auto f = glm::normalize(forward);

	auto r = glm::cross(f, vec3(0,1,0));
	vec3 u;

	if (glm::length(r) < 0.5f)
	{
		r = vec3(1,0,0);
		u = vec3(0,0,-1);
	}
	else
	{
		u = glm::cross(r, f);
	}

	return mat3(r.x, r.y, r.z,
		u.x, u.y, u.z,
		f.x, f.y, f.z);
};

float* vec3ToFloats(const vec3 v) {
	float arr[3]{ v.x, v.y, v.z };
	return &arr[0];
};

#define LITV3(v) {v.x,v.y,v.z}
#define UPKV3(v) v.x,v.y,v.z

union col32
{
	struct rgba
	{
		uint8_t r{ 255 };
		uint8_t g{ 255 };
		uint8_t b{ 255 };
		uint8_t a{ 255 };
		rgba(
			uint8_t r,
			uint8_t g,
			uint8_t b,
			uint8_t a = 255
		) :
			r(r), g(g), b(b), a(a)
		{}
	};
	uint32_t val;
	rgba channels;
	col32() = delete;
	col32(
		uint8_t r,
		uint8_t g,
		uint8_t b,
		uint8_t a = 255
	) {
		new(&channels) rgba(r,g,b,a);
	}
	static col32 red;
	static col32 orange;
	static col32 yellow;
	static col32 yellowgreen;
	static col32 green;
	static col32 cyan;
	static col32 blue;
	static col32 purple;
	static col32 magenta;
};
col32 col32::red(255, 0, 0);
col32 col32::orange(255, 122, 0);
col32 col32::yellow(255, 255, 0);
col32 col32::yellowgreen(122, 255, 0);
col32 col32::green(0, 255, 0);
col32 col32::cyan(0, 255, 255);
col32 col32::blue(0, 0, 255);
col32 col32::purple(150, 0, 255);
col32 col32::magenta(255, 0, 255);

struct AABB
{
	vec3 min, max;
	AABB(vec3 min, vec3 max) : min(min), max(max) {}
};

struct Transform
{
	vec3 scalexyz;
	quat rotation;
	vec3 position;

	mat4 GetMatrix() {
		auto m = mat4(rotation);
		m = scale(m, scalexyz);
		return translate(m, position);
	}
};

struct Frustum
{
	float fov;
	float nearPlane;
	float farPlane;
	float aspectRatio;

	mat4 GetFrustumMatrix(mat4 viewMatrix) {
		mat4 pm = glm::perspectiveFov(radians(fov), aspectRatio, 1.f, nearPlane, farPlane);
		mat4 vpm = pm * viewMatrix;
		return vpm;
	}

	mat4 GetFrustumMatrix(Transform holder) {
		return GetFrustumMatrix(inverse(holder.GetMatrix()));
	}
};

class TestBedBase : public bigg::Application
{
public:
	//debug camera
	float cameraYaw{ 0 };
	float cameraPitch{ 0 };
	float mouseLookSpeed{ 0.005f };
	float cameraMovementSpeed{ 2 };
	float cameraFov{ 80 };
	float nearClip{ 0.1f };
	float farClip{ 1000 };
	bool wasLockedLastFrame{ false };
	glm::vec3 cameraPosition{ glm::vec3(0,5,10) };

	static DebugDrawEncoder* dde;

	virtual void update_rewire(float dt) = 0;
	virtual void init_rewire() = 0;
	virtual void shutdown_rewire() = 0;

	void initialize(int _argc, char **_argv) override {
		//         glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); //doesnt work
		glfwSetKeyCallback(mWindow, key_callback); //my god glfw... just *my GAWD!*
		ddInit();
		dde = new DebugDrawEncoder();
		init_rewire();
	}

	void onReset() override {
		bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000030ff, 1.0f, 0);
		bgfx::setViewRect(0, 0, 0, uint16_t(getWidth()), uint16_t(getHeight()));
		// 		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}

private:
	void update(float dt) override {

		double xpos, ypos;
		glfwGetCursorPos(mWindow, &xpos, &ypos);

		// 		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		if (glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_RIGHT)) {
			if (wasLockedLastFrame) {
				cameraYaw += (xpos - (getWidth() / 2)) * mouseLookSpeed;
				cameraPitch += (ypos - (getHeight() / 2)) * mouseLookSpeed;
			}
			glfwSetCursorPos(mWindow, getWidth() / 2, getHeight() / 2);
			wasLockedLastFrame = true;
		}
		else
			wasLockedLastFrame = false;

		float inputVerticalAxis = glfwGetKey(mWindow, GLFW_KEY_W) ? 1 : (glfwGetKey(mWindow, GLFW_KEY_S) ? -1 : 0);
		float inputHorizontalAxis = glfwGetKey(mWindow, GLFW_KEY_A) ? 1 : (glfwGetKey(mWindow, GLFW_KEY_D) ? -1 : 0);
		float cameraSpeedMult = isShiftDown ? 5 : 1;

		mat4 cameraPitchMat = rotate(mat4(1), cameraPitch, vec3(1, 0, 0));
		mat4 cameraRotation = rotate(cameraPitchMat, cameraYaw, vec3(0, 1, 0));

		vec3 localCameraMovement = vec3(inputHorizontalAxis, 0, inputVerticalAxis) * cameraMovementSpeed * dt * cameraSpeedMult;
		vec3 worldCameraMovement = mat3(inverse(cameraRotation)) * -localCameraMovement;
		cameraPosition += worldCameraMovement;
		mat4 cameraTranslation = translate(mat4(1), -cameraPosition);

		mat4 viewMatrix = cameraRotation * cameraTranslation;

		glm::mat4 proj = bigg::perspective(glm::radians(cameraFov), float(getWidth()) / getHeight(), nearClip, farClip);
		bgfx::setViewTransform(0, &viewMatrix[0][0], &proj[0][0]);
		bgfx::setViewRect(0, 0, 0, uint16_t(getWidth()), uint16_t(getHeight()));
		bgfx::touch(0);
		
		ImGui::Begin("Testbed Controls");
		ImGui::TextWrapped("Hold the RMB to look around. Shift increases movement speed.");
		ImGui::SliderFloat("Movement Speed", &cameraMovementSpeed, 1, 10);
		ImGui::SliderFloat("Mouse Look Speed", &mouseLookSpeed, 0.001, 0.05);
		ImGui::SliderFloat("Field of View", &cameraFov, 20, 120, "%.0f");
		ImGui::SliderFloat("Near Clipping", &nearClip, 0.01f, farClip, "%.2f");
		ImGui::SliderFloat("Far Clipping", &farClip, nearClip, 10000, "%.0f");
		ImGui::End();

		delete dde;
		dde = new DebugDrawEncoder(); 

		dde->begin(0);

		update_rewire(dt);

		dde->end();
	}

	int shutdown() override {
		return Application::shutdown();
		shutdown_rewire();
	}

};

DebugDrawEncoder* TestBedBase::dde;

class TestBed : private TestBedBase {

public:
	virtual void Init() = 0;
	virtual void Update(float dt) = 0;
	virtual void Shutdown() = 0;
	
	int Run(int argc, char** argv) {
		return TestBed::run(argc, argv);
	}

	//getters
	DebugDrawEncoder* GetCurrentDDE() { return dde; };
	float GetUptime() { return upTime_; }

	static void DrawArrow(const vec3 start, const vec3 end, const col32 color,
		float headHeight = -0.25f, float headWidth = 0.05f)
	{
		auto vec = end - start;
		auto dir = glm::normalize(vec);

		auto m = CreateFromForwardVector(dir);

		float d = headWidth * length(vec);
		auto p0 = end + (m * vec3(0, d, headHeight *length(vec)));
		auto p1 = end + (m * vec3(0, -d, headHeight*length(vec)));
		auto p2 = end + (m * vec3(d, 0, headHeight *length(vec)));
		auto p3 = end + (m * vec3(-d, 0, headHeight*length(vec)));

		dde->setColor(color.val);

		DdVertex l[]{
			LITV3(start),LITV3(end),
			LITV3(end),LITV3(p0),
			LITV3(end),LITV3(p1),
			LITV3(end),LITV3(p2),
			LITV3(end),LITV3(p3),
			LITV3(p0), LITV3(p2),
			LITV3(p1), LITV3(p3),
			LITV3(p3), LITV3(p0),
			LITV3(p1), LITV3(p2),
			LITV3(p0), LITV3(p1),
			LITV3(p2), LITV3(p3),
		};

		dde->drawLineList(22, &l[0]);
	}

	static void DrawGrid(const vec3 point = vec3(0), uint size = 20) {
		dde->drawGrid(Axis::Y, vec3ToFloats(point), size);
	}

	static void DrawAxis(const vec3 point) { //todo rotation
		dde->drawAxis(UPKV3(point));
	}

	static void DrawOrb(const vec3 point, float size) { //todo rotation
		dde->drawOrb(UPKV3(point),size);
	}

	static void DrawSingleLine(const vec3 start, const vec3 end, const col32 color)
	{
		dde->setColor(color.val);

		DdVertex l[]{
			LITV3(start),LITV3(end)
		};

		dde->drawLineList(2, &l[0]);
	}

	static void DrawAABB(vec3 min, vec3 max) {
		Aabb _aabb = { LITV3(min), LITV3(max) };
		dde->setWireframe(true);
		dde->draw(_aabb);
	}

	static void DrawOBB(mat4 m, col32 color, bool drawAABB = false, bool wireframe = true) {
		Obb obb;
		memcpy(&obb.m_mtx, &m, sizeof m);
		dde->setColor(color.val);
		dde->setWireframe(wireframe);
		dde->draw(obb);

		if (drawAABB) {
			Aabb aabb;
			toAabb(aabb, obb);
			color.channels.a *= 0.5f;
			dde->setWireframe(true);
			dde->setColor(color.val);
			dde->draw(aabb);
		}
	}

	static void DrawSphere(vec3 center, float radius, col32 color, int lod = 3) {
		Sphere sphere = { { 0.0f, 5.0f, 0.0f }, 1.0f };
		dde->setColor(color.val);
		dde->setWireframe(true);
		dde->setLod(lod);
		dde->draw(sphere);
	}

	void DrawSphereAuto(vec3 center, float radius, col32 color) {
		Sphere sphere = { LITV3(center), radius };
		dde->setColor(color.val);
		dde->setWireframe(true);
		auto lodDist = (uint8_t)(bx::pow(length(cameraPosition - center)*0.15f, 1.5f)+0.8f);
		dde->setLod(lodDist);
		dde->draw(sphere);
	}
	
	void DrawFrustum(mat4 mat) {
		dde->drawFrustum(&mat);
	}

private:
	float upTime_{ 0 };
	
	virtual void update_rewire(float dt) override
	{
		upTime_ += dt;
		Update(dt);
	}

	virtual void init_rewire() override
	{
		Init();
	}

	virtual void shutdown_rewire() override
	{
		Shutdown();
	}

};

class FrustumTest : public TestBed {
	virtual void Init() override
	{
	}
	virtual void Update(float dt) override
	{
		DebugDrawEncoder* dde = GetCurrentDDE(); //to draw manually using bgfx debug draw
		
 		DrawGrid();

		DrawArrow(vec3(0), vec3(3), col32::purple);
		DrawAxis(vec3(1));
		DrawSingleLine(vec3(-1), vec3(-3), col32::red);
		DrawOrb(vec3(0, 2, 0),1);
		DrawAABB(vec3(3, 3, 3), vec3(4, 4, 4));
				
		Transform t{
			vec3(1),
			rotate(quat(1,0,0,0),GetUptime(),vec3(1)),
			vec3(2)
		};
		DrawOBB(t.GetMatrix(), col32::green, true);
		
		for (size_t i = 0; i < 10; i++)
		{
			for (size_t y = 0; y < 10; y++)
			{
				DrawSphereAuto(vec3(i*5, 0, y*5), 0.5f, col32::blue);
			}
		}
		dde->setColor(0xffffffff);

		DrawAxis(t.position);

		Frustum f{
			90,0.1,20,2
		};
		
		DrawFrustum(f.GetFrustumMatrix(t));





	}
	virtual void Shutdown() override
	{
	}
};

int main(int argc, char** argv)
{
	FrustumTest app;
	return app.Run(argc, argv);
}