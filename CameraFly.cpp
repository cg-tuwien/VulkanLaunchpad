/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#include "Camera.h"
#include <glm/gtx/quaternion.hpp>
#include <list>
#include <algorithm>
#include <unordered_map>

std::list<VklCamera> mCameras;
float mInput1 = 6.0f;
bool mInput2;
bool mInput3;
bool keyW;
bool keyA;
bool keyS;
bool keyD;
bool keySpace;
bool keyCtrl;
bool keyShift;
/*!
 *	This callback function gets invoked by GLFW during glfwPollEvents() if there was
 *	mouse button input that can be processed by our application.
 */
void mouseButtonCallbackFromGlfw(GLFWwindow* glfw_window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		mInput3 = true;
		std::cout << "mouse pressed";
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		mInput3 = false;
	}

	// Keep potentially previously set callbacks intact:
	for (const VklCamera& cam : mCameras) {
		if (nullptr != cam.mPreviousMouseButtonFun) {
			cam.mPreviousMouseButtonFun(glfw_window, button, action, mods);
		}
	}
}



void keyCallbackFromGlfw(GLFWwindow* glfw_window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_W && action == GLFW_PRESS) {
		keyW = true;
	}
	else if (key == GLFW_KEY_W && action == GLFW_RELEASE) {
		keyW = false;
	}
	else if (key == GLFW_KEY_A && action == GLFW_PRESS) {
		keyA = true;
	}
	else if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
		keyA = false;
	}
	else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
		keyS = true;
	}
	else if (key == GLFW_KEY_S && action == GLFW_RELEASE) {
		keyS = false;
	}
	else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
		keyD = true;
	}
	else if (key == GLFW_KEY_D && action == GLFW_RELEASE) {
		keyD = false;
	}
	else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		keySpace = true;
	}
	else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE) {
		keySpace = false;
	}
	else if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS) {
		keyCtrl = true;
	}
	else if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_RELEASE) {
		keyCtrl = false;
	}
	else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS) {
		keyShift = true;
	}
	else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE) {
		keyShift = false;
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

	auto previous_key_callback = glfwSetKeyCallback(window, keyCallbackFromGlfw);
	if (previous_key_callback == keyCallbackFromGlfw) {
		previous_key_callback = nullptr; // Do not store this if this is pointing to the same callback function; otherwise we get a StackOverflow
	}

	auto& newCam = mCameras.emplace_back(VklCamera{
		glm::mat4(1),
		projection_matrix,
		0.0, 0.0,
		0.0f, 0.0f,
		glm::vec3{0},
		window,
		previous_mouse_callback,
		previous_scroll_callback,
		previous_key_callback
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

void vklUpdateCamera(VklCameraHandle handle, double x, double y, bool moveForward, bool moveLeft, bool moveBackward, bool moveRight, bool moveUp, bool moveDown, bool sprint, double dt) {
	// Calculate mouse movement since last update
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}
	double dx = x - it->mMouseX;
	double dy = y - it->mMouseY;

	float mouseSensitivity = 2.0f;
	float baseMovementSpeed = 1.0f;
	float sprintMultiplier = sprint ? 2.0f : 1.0f;
	float movementSpeed = baseMovementSpeed * sprintMultiplier * dt;
	float verticalSpeed = 2.5f * dt;

	it->mYaw += dx * mouseSensitivity * dt;
	it->mPitch -= dy * mouseSensitivity * dt;

	it->mPitch = glm::clamp(it->mPitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);

	// Calculate the new forward, right, and up vectors
	glm::vec3 forward = glm::normalize(glm::vec3(cos(it->mYaw) * cos(it->mPitch), sin(it->mPitch), sin(it->mYaw) * cos(it->mPitch)));
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

	if (moveForward)
		it->mPosition += forward * movementSpeed;
	if (moveBackward)
		it->mPosition -= forward * movementSpeed;
	if (moveLeft)
		it->mPosition -= right * movementSpeed;
	if (moveRight)
		it->mPosition += right * movementSpeed;
	if (moveUp)
		it->mPosition += glm::vec3(0.0f, 1.0f, 0.0f) * verticalSpeed;
	if (moveDown)
		it->mPosition -= glm::vec3(0.0f, 1.0f, 0.0f) * verticalSpeed;

	it->mViewMatrix = glm::lookAt(it->mPosition, it->mPosition + forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Y is up
	it->mMouseX = x;
	it->mMouseY = y;
}


void vklUpdateCamera(VklCameraHandle handle, dobule dt) {
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	double x, y;
	glfwGetCursorPos(it->mWindow, &x, &y);
	vklUpdateCamera(handle, x, y, keyW, keyA, keyS, keyD, keySpace, keyCtrl, keyShift, dt);
}

