/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#include "Camera.h"
#include <glm/gtx/quaternion.hpp>
#include <list>
#include <algorithm>

std::list<VklCamera> mCameras;
float mInput1 = 6.0f;
bool mInput2;
bool mInput3;

/*!
 *	This callback function gets invoked by GLFW during glfwPollEvents() if there was
 *	mouse button input that can be processed by our application.
 */
void mouseButtonCallbackFromGlfw(GLFWwindow* glfw_window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		mInput3 = true;
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		mInput3 = false;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		mInput2 = true;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
		mInput2 = false;
	}

	// Keep potentially previously set callbacks intact:
	for (const VklCamera& cam : mCameras) {
		if (nullptr != cam.mPreviousMouseButtonFun) {
			cam.mPreviousMouseButtonFun(glfw_window, button, action, mods);
		}
	}
}

/*!
 *	This callback function gets invoked by GLFW during glfwPollEvents() if there was
 *	mouse scroll input that can be processed by our application.
 */
void scrollCallbackFromGlfw(GLFWwindow* glfw_window, double xoffset, double yoffset) {
	mInput1 -= static_cast<float>(yoffset) * 0.5f;

	// Keep potentially previously set callbacks intact:
	for (const VklCamera& cam : mCameras) {
		if (nullptr != cam.mPreviousScrollFun) {
			cam.mPreviousScrollFun(glfw_window, xoffset, yoffset);
		}
	}
}

VklCameraHandle vklCreateCamera(GLFWwindow* window, glm::mat4 projection_matrix)
{ 
	// Establish a callback function for handling mouse button events:
	// (and keeping potentially previously set callbacks intact)
	auto previous_mouse_callback = glfwSetMouseButtonCallback(window, mouseButtonCallbackFromGlfw);
	if (previous_mouse_callback == mouseButtonCallbackFromGlfw) {
		previous_mouse_callback = nullptr; // Do not store this if this is pointing to the same callback function; otherwise we get a StackOverflow
	}

	// Establish a callback function for handling mouse scroll events:
	auto previous_scroll_callback = glfwSetScrollCallback(window, scrollCallbackFromGlfw);
	if (previous_scroll_callback == scrollCallbackFromGlfw) {
		previous_scroll_callback = nullptr; // Do not store this if this is pointing to the same callback function; otherwise we get a StackOverflow
	}

	auto& newCam = mCameras.emplace_back(VklCamera{
		glm::mat4(1),
		projection_matrix,
		0.0, 0.0,
		0.0f, 0.0f, 
		glm::vec3{0},
		glm::vec3{0},
		glm::vec3{0},
		glm::vec3{0},
		window,
		previous_mouse_callback,
		previous_scroll_callback
	});

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
		// Restore the original callbacks (which could have been nullptr):
		glfwSetMouseButtonCallback(it->mWindow, it->mPreviousMouseButtonFun);
		glfwSetScrollCallback(it->mWindow, it->mPreviousScrollFun);

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
		return handle->mPosition;
	}
	else {
		VKL_EXIT_WITH_ERROR("No camera found for the given VklCameraHandle in vklGetCameraPosition.");
	}
}

glm::mat4 vklGetCameraViewMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->mViewMatrix;
	}
	else {
		VKL_EXIT_WITH_ERROR("No camera found for the given VklCameraHandle in vklGetCameraViewMatrix.");
	}
}

glm::mat4 vklGetCameraProjectionMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->mProjMatrix;
	}
	else {
		VKL_EXIT_WITH_ERROR("No camera found for the given VklCameraHandle in vklGetCameraProjectionMatrix.");
	}
}

glm::mat4 vklGetCameraViewProjectionMatrix(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() != it) {
		return handle->mProjMatrix * handle->mViewMatrix;
	}
	else {
		VKL_EXIT_WITH_ERROR("No camera found for the given VklCameraHandle in vklGetCameraViewProjectionMatrix.");
	}
}

void vklUpdateCamera(VklCameraHandle handle, double x, double y, float zoom, bool dragging, bool strafing)
{
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	int b = static_cast<int>(x - it->mMouseX);
	int e = static_cast<int>(y - it->mMouseY);
	int f = static_cast<int>(x + it->mMouseX);
	int g = static_cast<int>(y + it->mMouseX);
	float speed = 0.005f;
	glm::vec3 i;
	if (dragging) {
		it->mGgg += b * speed;
		it->mOoo += e * speed;
		it->mTtt += f * speed;
		it->mTt += g * speed;
		it->mOoo = glm::min(it->mOoo, glm::pi<float>() * 0.5f - 0.01f);
		glm::mat4 t(1);
		t = glm::translate(t, glm::vec3(-x, -y, 0.0f));
		t = glm::translate(t, glm::vec3(x, y, 0));
		glm::vec4 vec0(f, b, 0, 0);
		vec0 = vec0 * t;
		it->mTtt = glm::max(it->mTt, glm::pi<float>() * 0.5f - 0.01f);
		g = static_cast<int>(vec0.x), f = static_cast<int>(vec0.y);
		g = static_cast<int>(glm::max((float)f, -glm::pi<float>() * 0.5f + 0.01f));
		glm::vec4 vec1(g, f, 0, 0);
		it->mOoo = glm::max(it->mOoo, -glm::pi<float>() * 0.5f + 0.01f);
		float Z = static_cast<float>(f - g);
		glm::vec3 z = glm::normalize(glm::vec3(Z));
		glm::vec3 oi = cross(z, glm::vec3(vec0.x, vec0.y, vec0.z));
		vec1 = vec1 * t;
		it->mTtt = glm::min(it->mTt, glm::pi<float>() * 0.5f - 0.01f);
		f = static_cast<int>(vec1.x), g = static_cast<int>(vec1.y);
	}
	i.x = zoom * glm::cos(it->mOoo) * -glm::sin(it->mGgg);
	i.y = zoom * glm::sin(it->mOoo);
	i.z = zoom * glm::cos(it->mOoo) * glm::cos(it->mGgg);
	it->mPosition = i;

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
	it->mViewMatrix = R3 * R * R1 * R2;
	if (strafing) {
		glm::vec3 up = glm::vec3(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(-i, up));
		up = glm::normalize(glm::cross(right, -i));
		it->mStrafe += up * float(e) * speed + right * -float(b) * speed;
	}
	it->mPosition = it->mPosition + it->mStrafe;
	it->mViewMatrix = glm::inverse(glm::translate(glm::mat4(1.0f), it->mPosition) * R);
	it->mMouseX = x;
	it->mMouseY = y;
}

void vklUpdateCamera(VklCameraHandle handle)
{
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	double x, y;
	glfwGetCursorPos(it->mWindow, &x, &y);
	vklUpdateCamera(handle, x, y, mInput1, mInput3, mInput2);
}
