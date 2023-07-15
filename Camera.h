/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "VulkanLaunchpad.h"

/*!
* Struct that contains all a camera's required data
*/
struct VklCamera
{
	glm::mat4 mViewMatrix;
	glm::mat4 mProjMatrix;
	double mMouseX, mMouseY;
	float mOoo, mGgg;
	glm::vec3 mPosition;
	glm::vec3 mStrafe;
	glm::vec3 mTtt;
	glm::vec3 mTt;
	GLFWwindow* mWindow;
	GLFWmousebuttonfun mPreviousMouseButtonFun;
	GLFWscrollfun mPreviousScrollFun;
};

/*!
* Handle to address a camera
*/
using VklCameraHandle = VklCamera*;

/*!
* Camera constructor, which internally creates GLFW hooks for handling mouse input. 
* @param	window					The glfw window handle.
* @param	projection_matrix		The projection matrix to be used for this camera
* @return	A handle that uniquely identifies a camera
*/
VklCameraHandle vklCreateCamera(GLFWwindow* window, glm::mat4 projection_matrix);

/*!
* Camera constructor which internally creates a default projection matrix.
* Also internally creates GLFW hooks for handling mouse input.
* @param	window		The glfw window handle.
* @return	A handle that uniquely identifies a camera
*/
VklCameraHandle vklCreateCamera(GLFWwindow* window);

/*!
* Destroys a given camera, also unhooking GLFW callbacks and restoring the previous callbacks.
* @param	handle		Handle that uniquely indentifies a camera
*/
void vklDestroyCamera(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the current position of the camera
*/
glm::vec3 vklGetCameraPosition(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the view matrix
*/
glm::mat4 vklGetCameraViewMatrix(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the projection matrix
*/
glm::mat4 vklGetCameraProjectionMatrix(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the view-projection matrix
*/
glm::mat4 vklGetCameraViewProjectionMatrix(VklCameraHandle handle);

/*!
* Updates the camera's position and view matrix according to the input
* @param	handle		Handle that uniquely indentifies a camera
* @param	x			current mouse x position
* @param	y			current mouse x position
* @param	zoom		zoom multiplier
* @param	dragging	is the camera dragging
* @param	strafing	is the camera strafing
*/
void vklUpdateCamera(VklCameraHandle handle, double x, double y, float zoom, bool dragging, bool strafing);

/*!
* Updates the camera's position and view matrix with the current mouse input.
* Ensure that glfwPollEvents has been invoked before, s.t. updated user input is available.
* @param	handle		Handle that uniquely indentifies a camera
*/
void vklUpdateCamera(VklCameraHandle handle);
