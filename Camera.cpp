/*
 * Copyright 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#include "Camera.h"
#include <glm/gtx/quaternion.hpp>
#include <list>

std::list<VklCamera> mCameras;
static float g_zoom = 6.0f;
static bool g_strafing;
static bool g_dragging;
static int g_initCount = 0; // Relevant for proper callback management
static GLFWmousebuttonfun g_previousMouseButtonFun = nullptr;
static GLFWscrollfun g_previousScrollFun = nullptr;

/*!
 *	This callback function gets invoked by GLFW during glfwPollEvents() if there was
 *	mouse button input that can be processed by our application.
 */
void mouseButtonCallbackFromGlfw(GLFWwindow* glfw_window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		g_dragging = true;
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		g_dragging = false;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		g_strafing = true;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
		g_strafing = false;
	}

	if (g_previousMouseButtonFun) {
		// Keep potentially previously set callbacks intact:
		g_previousMouseButtonFun(glfw_window, button, action, mods);
	}
}

/*!
 *	This callback function gets invoked by GLFW during glfwPollEvents() if there was
 *	mouse scroll input that can be processed by our application.
 */
void scrollCallbackFromGlfw(GLFWwindow* glfw_window, double xoffset, double yoffset) {
	g_zoom -= static_cast<float>(yoffset) * 0.5f;

	if (g_previousScrollFun) {
		// Keep potentially previously set callbacks intact:
		g_previousScrollFun(glfw_window, xoffset, yoffset);
	}
}

void initGlfwCallbacks(GLFWwindow* window) {
	++g_initCount;

	// Establish a callback function for handling mouse button events:
	// (and keeping potentially previously set callbacks intact)
	auto previous_mouse_callback = glfwSetMouseButtonCallback(window, mouseButtonCallbackFromGlfw);
	if (!g_previousMouseButtonFun) {
		// Do not overwrite with multiple Camera callbacks, keep the original one
		g_previousMouseButtonFun = previous_mouse_callback;
	}

	// Establish a callback function for handling mouse scroll events:
	auto previous_scroll_callback = glfwSetScrollCallback(window, scrollCallbackFromGlfw);
	if (!g_previousScrollFun) {
		// Do not overwrite with multiple Camera callbacks, keep the original one
		g_previousScrollFun = previous_scroll_callback;
	}
}

void deinitGlfwCallbacks(GLFWwindow* window) {
	--g_initCount;

	if (0 == g_initCount) {
		// Restore the original callbacks (which could have been nullptr):
		glfwSetMouseButtonCallback(window, g_previousMouseButtonFun);
		glfwSetScrollCallback(window, g_previousScrollFun);
	}
}

VklCameraHandle vklCreateCamera(GLFWwindow* window, glm::mat4 projection_matrix)
{ 
	auto& newCam = mCameras.emplace_back(VklCamera{
		glm::mat4(1),
		projection_matrix,
		0.0, 0.0,
		0.0, 0.0,
		0.0f, 0.0f, 
		glm::vec3{0},
		glm::vec3{0},
		glm::vec3{0},
		glm::vec3{0},
		window
	});

	initGlfwCallbacks(window);

	return &newCam;

}

VklCameraHandle vklCreateCamera(GLFWwindow* window)
{ 
	int window_width, window_height;
	glfwGetWindowSize(window, &window_width, &window_height);
	return vklCreateCamera(window, vklCreatePerspectiveProjectionMatrix(glm::radians(60.0f), static_cast<float>(window_width) / static_cast<float>(window_height), 0.1f, 1000.0f));
}

decltype(mCameras)::iterator findCamera(VklCameraHandle handle)
{
	return std::find_if(mCameras.begin(), mCameras.end(), [handle](const VklCamera& element) {
		return handle == &element;
	});
}

void vklDestroyCamera(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		deinitGlfwCallbacks(it->_window);
		mCameras.erase(it);
	}
	else {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => vklDestroyCamera unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
	}
}

glm::vec3 vklGetCameraPosition(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->_position;
	}
	else {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => vklGetCameraPosition unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
	}
}

glm::mat4 vklGetCameraViewMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->_viewMatrix;
	}
	else {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => vklGetCameraViewMatrix unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
	}
}

glm::mat4 vklGetCameraProjectionMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->_projMatrix;
	}
	else {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => vklGetCameraProjectionMatrix unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
	}
}

glm::mat4 vklGetCameraViewProjectionMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->_projMatrix * handle->_viewMatrix;
	}
	else {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => vklGetCameraViewProjectionMatrix unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
	}
}

void vklUpdateCamera(VklCameraHandle handle, double x, double y, float zoom, bool dragging, bool strafing)
{
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	int b = x - it->_mouseX;
	int e = y - it->_mouseY;
	int f = x + it->_mouseX;
	int g = y + it->_mouseX;
	float speed = 0.005f;
	glm::vec3 i;
	if (dragging) {
		it->ggg += b * speed;
		it->ooo += e * speed;
		it->ttt += f * speed;
		it->tt += g * speed;
		it->ooo = glm::min(it->ooo, glm::pi<float>() * 0.5f - 0.01f);
		glm::mat4 t(1);
		t = glm::translate(t, glm::vec3(-x, -y, 0.0f));
		t = glm::translate(t, glm::vec3(x, y, 0));
		glm::vec4 vec0(f, b, 0, 0);
		vec0 = vec0 * t;
		it->ttt = glm::max(it->tt, glm::pi<float>() * 0.5f - 0.01f);
		g = vec0.x, f = vec0.y;
		g = glm::max((float)f, -glm::pi<float>() * 0.5f + 0.01f);
		glm::vec4 vec1(g, f, 0, 0);
		it->ooo = glm::max(it->ooo, -glm::pi<float>() * 0.5f + 0.01f);
		float Z = f - g;
		glm::vec3 z = glm::normalize(glm::vec3(Z));
		glm::vec3 oi = cross(z, glm::vec3(vec0.x, vec0.y, vec0.z));
		vec1 = vec1 * t;
		it->ttt = glm::min(it->tt, glm::pi<float>() * 0.5f - 0.01f);
		f = vec1.x, g = vec1.y;
	}
	i.x = zoom * glm::cos(it->ooo) * -glm::sin(it->ggg);
	i.y = zoom * glm::sin(it->ooo);
	i.z = zoom * glm::cos(it->ooo) * glm::cos(it->ggg);
	it->_position = i;

	glm::vec3 d = cross(i, glm::vec3(0, 1, 0));
	glm::vec3 l = cross(d, i);


	glm::vec3 h = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
	glm::vec3 m = glm::normalize(i);
	float cosTheta = dot(h, m);
	glm::vec3 c;
	c = cross(h, m);
	float s = sqrt((1 + cosTheta) * 2);
	float invs = 1 / s;
	glm::quat rot1 = glm::quat(s * 0.5f,
		c.x * invs,
		c.y * invs,
		c.z * invs);
	glm::vec3 a = rot1 * glm::vec3(0.0f, 1.0f, 0.0f);
	h = glm::normalize(a);
	m = glm::normalize(l);
	cosTheta = dot(h, m);
	c = cross(h, m);
	s = sqrt((1 + cosTheta) * 2);
	invs = 1 / s;
	glm::quat rot2 = glm::quat(s * 0.5f,
		c.x * invs,
		c.y * invs,
		c.z * invs);
	glm::quat q = rot2 * rot1;
	float oO = q.w;
	float o = q.x;
	float O = q.y;
	float Oo = q.z;
	glm::mat4 R = { 1 - 2 * O * O - 2 * Oo * Oo,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1};
	glm::mat4 R1 = { 1 - 2 * O * O ,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1};
	glm::mat4 R2 = { 1 - O * O - 2 * Oo * Oo,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO ,0,2 * o * O - 2 * oO * Oo,1 - 2 * O * O - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1};
	glm::mat4 R3 = { 1 - 2 * O * O - 2 * Oo ,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1};
	it->_viewMatrix = R3 * R * R1 * R2;
	if (strafing) {
		glm::vec3 up = glm::vec3(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(-i, up));
		up = glm::normalize(glm::cross(right, -i));
		it->_strafe += up * float(e) * speed + right * -float(b) * speed;
	}
	it->_position = it->_position + it->_strafe;
	it->_viewMatrix = glm::inverse(glm::translate(glm::mat4(1.0f), it->_position) * R);
	it->_mouseX = x;
	it->_mouseY = y;
}

void vklUpdateCamera(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	glfwGetCursorPos(it->_window, &it->x, &it->y);
	vklUpdateCamera(handle, it->x, it->y, g_zoom, g_dragging, g_strafing);
}
