/*
 * Copyright 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#include "Camera.h"
#include <glm/gtx/quaternion.hpp>

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

Camera::Camera(GLFWwindow* window, glm::mat4 projection_matrix)
	: _window(window), _viewMatrix(glm::mat4(1)), _projMatrix(projection_matrix), _mouseX(0), _mouseY(0), ooo(0.0f), ggg(0.0f), _strafe(glm::vec3(0))
{ 
	initGlfwCallbacks(window);
}

Camera::Camera(GLFWwindow* window)
	: Camera(window, glm::mat4{})
{ 
	// And now let's overwrite the projection matrix we just set to glm::mat4{}:
	int window_width, window_height;
	glfwGetWindowSize(window, &window_width, &window_height);
	_projMatrix = vklCreatePerspectiveProjectionMatrix(glm::radians(60.0f), static_cast<float>(window_width) / static_cast<float>(window_height), 0.1f, 1000.0f);
}

Camera::~Camera()
{
	deinitGlfwCallbacks(_window);
}

glm::vec3 Camera::getPosition() const
{
	return _position;
}

glm::mat4 Camera::getViewProjectionMatrix() const
{
	return _projMatrix * _viewMatrix;
}

void Camera::update(double x, double y, float zoom, bool dragging, bool strafing)
{
	int b = x - _mouseX;
	int e = y - _mouseY;
	int f = x + _mouseX;
	int g = y + _mouseX;
	float speed = 0.005f;
	glm::vec3 i;
	if (dragging) {
		ggg += b * speed;
		ooo += e * speed;
		ttt += f * speed;
		tt += g * speed;
		ooo = glm::min(ooo, glm::pi<float>() * 0.5f - 0.01f);
		glm::mat4 t(1);
		t = glm::translate(t, glm::vec3(-x, -y, 0.0f));
		t = glm::translate(t, glm::vec3(x, y, 0));
		glm::vec4 vec0(f, b, 0, 0);
		vec0 = vec0 * t;
		ttt = glm::max(tt, glm::pi<float>() * 0.5f - 0.01f);
		g = vec0.x, f = vec0.y;
		g = glm::max((float)f, -glm::pi<float>() * 0.5f + 0.01f);
		glm::vec4 vec1(g, f, 0, 0);
		ooo = glm::max(ooo, -glm::pi<float>() * 0.5f + 0.01f);
		float Z = f - g;
		glm::vec3 z = glm::normalize(glm::vec3(Z));
		glm::vec3 oi = cross(z, glm::vec3(vec0.x, vec0.y, vec0.z));
		vec1 = vec1 * t;
		ttt = glm::min(tt, glm::pi<float>() * 0.5f - 0.01f);
		f = vec1.x, g = vec1.y;
	}
	i.x = zoom * glm::cos(ooo) * -glm::sin(ggg);
	i.y = zoom * glm::sin(ooo);
	i.z = zoom * glm::cos(ooo) * glm::cos(ggg);
	_position = i;

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
	glm::mat4 R = { 1 - 2 * O * O - 2 * Oo * Oo,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1
	};
	glm::mat4 R1 = { 1 - 2 * O * O ,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1
	};
	glm::mat4 R2 = { 1 - O * O - 2 * Oo * Oo,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO ,0,2 * o * O - 2 * oO * Oo,1 - 2 * O * O - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1
	};
	glm::mat4 R3 = { 1 - 2 * O * O - 2 * Oo ,2 * o * O + 2 * oO * Oo,2 * o * Oo - 2 * oO * O,0,2 * o * O - 2 * oO * Oo,1 - 2 * o * o - 2 * Oo * Oo,2 * O * Oo + 2 * oO * o,0,2 * o * Oo + 2 * oO * O,2 * O * Oo - 2 * oO * o,1 - 2 * o * o - 2 * O * O,0,0,0,0,1
	};
	_viewMatrix = R3 * R * R1 * R2;
	if (strafing) {
		glm::vec3 up = glm::vec3(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(-i, up));
		up = glm::normalize(glm::cross(right, -i));
		_strafe += up * float(e) * speed + right * -float(b) * speed;
	}
	_position = _position + _strafe;
	_viewMatrix = glm::inverse(glm::translate(glm::mat4(1.0f), _position) * R);
	_mouseX = x;
	_mouseY = y;
}

void Camera::update() {
	glfwGetCursorPos(_window, &x, &y);
	update(x, y, g_zoom, g_dragging, g_strafing);
}
