/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#include "Camera.h"
#include <glm/gtx/quaternion.hpp>
#include <list>
#include <algorithm>
#include <unordered_map>

std::list<VklCamera> mCameras;
bool keyW;
bool keyA;
bool keyS;
bool keyD;
bool keySpace;
bool keyCtrl;
bool keyShift;

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

VklCameraHandle vklCreateCamera(GLFWwindow* window, glm::mat4 projection_matrix)
{
	// Establish a callback function for handling key button events:
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
		glfwSetKeyCallback(it->mWindow, it->mPreviousKeyFun);
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

	it->mViewMatrix = glm::lookAt(it->mPosition, it->mPosition + forward, glm::vec3(0.0f, 1.0f, 0.0f));
	it->mMouseX = x;
	it->mMouseY = y;
}


void vklUpdateCamera(VklCameraHandle handle, double dt) {
	auto it = findCamera(handle);

	if (mCameras.end() == it) {
		std::cout << "WARNING: No camera found for handle[" << handle << "] => update unsuccessful." << VKL_DESCRIBE_FILE_LOCATION_FOR_OUT_STREAM << std::endl;
		return;
	}

	double x, y;
	glfwGetCursorPos(it->mWindow, &x, &y);
	vklUpdateCamera(handle, x, y, keyW, keyA, keyS, keyD, keySpace, keyCtrl, keyShift, dt);
}

