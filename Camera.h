/*
 * Copyright 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

/*!
 * Arc ball camera, modified by mouse input
 */
class Camera
{	
public:
	/*!
	 * Camera constructor
	 * @param	projection_matrix		The projection matrix to be used for this camera
	 */
	Camera(glm::mat4 projection_matrix);

	/*!
	 * Camera constructor which internally creates a default projection matrix using the 
	 * aspect ratio calculated from the given window_width and window_height parameters.
	 * @param	window_width 		The width  of your surface to calculate the aspect ratio for the projection matrix.
	 * @param	window_height		The height of your surface to calculate the aspect ratio for the projection matrix.
	 */
	Camera(int window_width, int window_height);

	/*!
	 * Camera constructor which internally creates a default projection matrix.
	 */
	Camera();

	virtual ~Camera();

	/*!
	 * @return the current position of the camera
	 */
	glm::vec3 getPosition() const;

	/*!
	 * @return the view-projection matrix
	 */
	glm::mat4 getViewProjectionMatrix() const;

		/*!
	 * Updates the camera's position and view matrix according to the input
	 * @param x			current mouse x position
	 * @param y			current mouse x position
	 * @param zoom		zoom multiplier
	 * @param dragging	is the camera dragging
	 * @param strafing	is the camera strafing
	 */
	void update(double x, double y, float zoom, bool dragging, bool strafing);

protected:
	glm::mat4 _viewMatrix;
	glm::mat4 _projMatrix;
	double _mouseX, _mouseY;
	float ooo, ggg;
	glm::vec3 _position;
	glm::vec3 _strafe;
	glm::vec3 ttt;
	glm::vec3 tt;
};