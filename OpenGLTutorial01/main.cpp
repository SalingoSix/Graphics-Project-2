#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SOIL2/SOIL2.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "cShaderProgram.h"
#include "cCamera.h"
#include "cMesh.h"
#include "cModel.h"
#include "cSkybox.h"
#include "cSkinnedMesh.h"
#include "cSkinnedGameObject.h"
#include "cAnimationState.h"
#include "cScreenQuad.h"
#include "cPlaneObject.h"
#include "cFrameBuffer.h"

//Setting up a camera GLOBAL
cCamera Camera(glm::vec3(0.0f, 0.0f, 3.0f),		//Camera Position
			glm::vec3(0.0f, 1.0f, 0.0f),		//World Up vector
			0.0f,								//Pitch
			-90.0f);							//Yaw

cCamera defaultCamera(glm::vec3(0.0f, 0.0f, 3.0f),		//Camera Position
	glm::vec3(0.0f, 1.0f, 0.0f),		//World Up vector
	0.0f,								//Pitch
	-90.0f);							//Yaw
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool firstMouse = true;
float lastX = 400, lastY = 300;

unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 800;

int drawType = 1;

std::map<std::string, cModel*> mapModelsToNames;
std::map<std::string, cSkinnedMesh*> mapSkinnedMeshToNames;
std::map<std::string, cShaderProgram*> mapShaderToName;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
unsigned int loadCubeMap(std::string directory, std::vector<std::string> faces);

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialzed GLAD" << std::endl;
		return -1;
	}

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

	//Setting up global openGL state
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	//Set up all our programs
	cShaderProgram* myProgram = new cShaderProgram();
	myProgram->compileProgram("assets/shaders/", "vertShader.glsl", "fragShader.glsl");
	mapShaderToName["mainProgram"] = myProgram;

	myProgram = new cShaderProgram();
	myProgram->compileProgram("assets/shaders/", "animVert.glsl", "animFrag.glsl");
	mapShaderToName["skinProgram"] = myProgram;

	myProgram = new cShaderProgram();
	myProgram->compileProgram("assets/shaders/", "skyBoxVert.glsl", "skyBoxFrag.glsl");
	mapShaderToName["skyboxProgram"] = myProgram;

	myProgram = new cShaderProgram();
	myProgram->compileProgram("assets/shaders/", "modelVert.glsl", "modelFrag.glsl");
	mapShaderToName["simpleProgram"] = myProgram;

	myProgram = new cShaderProgram();
	myProgram->compileProgram("assets/shaders/", "quadVert.glsl", "quadFrag.glsl");
	mapShaderToName["quadProgram"] = myProgram;

	//Assemble all our models
	std::string path = "assets/models/apple/apple textured obj.obj";
	mapModelsToNames["Apple"] = new cModel(path);

	path = "assets/models/banana/banana.obj";
	mapModelsToNames["Banana"] = new cModel(path);

	path = "assets/models/pumpkin/PumpkinOBJ.obj";
	mapModelsToNames["Pumpkin"] = new cModel(path);

	path = "assets/models/bean/chicago bean.obj";
	mapModelsToNames["Bean"] = new cModel(path);

	//Creating two frame buffers: one to display within the scene, and one that displays the whole scene
	cFrameBuffer mainFrameBuffer(SCR_HEIGHT, SCR_WIDTH);
	cFrameBuffer miniFrameBuffer(SCR_HEIGHT, SCR_WIDTH);

	//Some simple shapes
	cScreenQuad screenQuad;
	cPlaneObject planeObject;

	//Positions for some of the point light
	glm::vec3 pointLightPositions[] = {
		glm::vec3(0.7f,  0.2f,  2.0f),
		glm::vec3(2.3f, -3.3f, -4.0f),
		glm::vec3(-4.0f,  2.0f, -12.0f),
		glm::vec3(0.0f,  0.0f, -3.0f)
	};

	//Load the skyboxes
	cSkybox skybox("assets/textures/skybox/");
	cSkybox spacebox("assets/textures/spacebox/");

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	mapShaderToName["skyboxProgram"]->useProgram();
	mapShaderToName["skyboxProgram"]->setInt("skybox", 0);

	mapShaderToName["mainProgram"]->useProgram();
	mapShaderToName["mainProgram"]->setInt("skybox", 0);

	mapShaderToName["quadProgram"]->useProgram();
	mapShaderToName["quadProgram"]->setInt("screenTexture", 0);

	//Before we start looping, save the one frame buffer texture
	glBindFramebuffer(GL_FRAMEBUFFER, miniFrameBuffer.FBO);

	glEnable(GL_DEPTH_TEST);
	glStencilMask(0x00);

	//All the rendering commands go in here
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 staticProjection;
	glm::mat4 staticView;
	glm::mat4 staticskyBoxView;

	staticProjection = glm::perspective(glm::radians(defaultCamera.zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	staticView = defaultCamera.getViewMatrix();
	staticskyBoxView = glm::mat4(glm::mat3(defaultCamera.getViewMatrix()));

	// view/projection transformations
	mapShaderToName["mainProgram"]->useProgram();
	mapShaderToName["mainProgram"]->setMat4("projection", staticProjection);
	mapShaderToName["mainProgram"]->setMat4("view", staticView);
	mapShaderToName["mainProgram"]->setInt("reflectRefract", 0);

	//Light settings go in here until I've made a class for them
	{
		//http://devernay.free.fr/cours/opengl/materials.html
		mapShaderToName["mainProgram"]->setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
		mapShaderToName["mainProgram"]->setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
		mapShaderToName["mainProgram"]->setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
		mapShaderToName["mainProgram"]->setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

		mapShaderToName["mainProgram"]->setVec3("pointLights[0].position", pointLightPositions[0]);
		mapShaderToName["mainProgram"]->setFloat("pointLights[0].constant", 1.0f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[0].linear", 0.09f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[0].quadratic", 0.032f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);

		mapShaderToName["mainProgram"]->setVec3("pointLights[1].position", pointLightPositions[1]);
		mapShaderToName["mainProgram"]->setFloat("pointLights[1].constant", 1.0f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[1].linear", 0.09f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[1].quadratic", 0.032f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[1].ambient", 0.05f, 0.05f, 0.05f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[1].diffuse", 0.8f, 0.8f, 0.8f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[1].specular", 1.0f, 1.0f, 1.0f);

		mapShaderToName["mainProgram"]->setVec3("pointLights[2].position", pointLightPositions[2]);
		mapShaderToName["mainProgram"]->setFloat("pointLights[2].constant", 1.0f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[2].linear", 0.09f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[2].quadratic", 0.032f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[2].ambient", 0.05f, 0.05f, 0.05f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[2].diffuse", 0.8f, 0.8f, 0.8f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[2].specular", 1.0f, 1.0f, 1.0f);

		mapShaderToName["mainProgram"]->setVec3("pointLights[3].position", pointLightPositions[3]);
		mapShaderToName["mainProgram"]->setFloat("pointLights[3].constant", 1.0f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[3].linear", 0.09f);
		mapShaderToName["mainProgram"]->setFloat("pointLights[3].quadratic", 0.032f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[3].ambient", 0.05f, 0.05f, 0.05f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[3].diffuse", 0.8f, 0.8f, 0.8f);
		mapShaderToName["mainProgram"]->setVec3("pointLights[3].specular", 1.0f, 1.0f, 1.0f);

		mapShaderToName["mainProgram"]->setVec3("spotLight.position", defaultCamera.position);
		mapShaderToName["mainProgram"]->setVec3("spotLight.direction", defaultCamera.front);
		mapShaderToName["mainProgram"]->setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
		mapShaderToName["mainProgram"]->setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));
		mapShaderToName["mainProgram"]->setFloat("spotLight.constant", 1.0f);
		mapShaderToName["mainProgram"]->setFloat("spotLight.linear", 0.09f);
		mapShaderToName["mainProgram"]->setFloat("spotLight.quadratic", 0.032f);
		mapShaderToName["mainProgram"]->setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
		mapShaderToName["mainProgram"]->setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
		mapShaderToName["mainProgram"]->setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
		//http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
	}

	// render the loaded model
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(1.0f, 0.0f, -2.0f)); // translate it down so it's at the center of the scene
	model = glm::scale(model, glm::vec3(0.4f, 0.4f, 0.4f));	// it's a bit too big for our scene, so scale it down
	mapShaderToName["mainProgram"]->setMat4("model", model);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
	mapModelsToNames["Banana"]->Draw(*mapShaderToName["mainProgram"]);

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(1.0f, 0.0f, -3.0f));
	model = glm::scale(model, glm::vec3(0.012f));
	mapShaderToName["mainProgram"]->setMat4("model", model);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
	mapModelsToNames["Apple"]->Draw(*mapShaderToName["mainProgram"]);

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-1.0f, 0.4f, -4.0f));
	model = glm::scale(model, glm::vec3(0.01f));
	mapShaderToName["mainProgram"]->setMat4("model", model);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
	mapModelsToNames["Pumpkin"]->Draw(*mapShaderToName["mainProgram"]);

	//Drawing the skybox
	mapShaderToName["skyboxProgram"]->useProgram();

	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	mapShaderToName["skyboxProgram"]->setMat4("projection", staticProjection);
	mapShaderToName["skyboxProgram"]->setMat4("view", staticskyBoxView);

	glBindVertexArray(skybox.VAO);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthFunc(GL_LESS); // set depth function back to default

	//Restore these values for the rest of the models we render
	mapShaderToName["mainProgram"]->useProgram();
	mapShaderToName["mainProgram"]->setVec3("spotLight.position", Camera.position);
	mapShaderToName["mainProgram"]->setVec3("spotLight.direction", Camera.front);
	mapShaderToName["mainProgram"]->setInt("reflectRefract", 0);

	while (!glfwWindowShouldClose(window))
	{
		processInput(window);

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		mapShaderToName["mainProgram"]->useProgram();
		mapShaderToName["mainProgram"]->setVec3("cameraPos", Camera.position);

		glm::mat4 projection = glm::perspective(glm::radians(Camera.zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = Camera.getViewMatrix();
		glm::mat4 skyboxView = glm::mat4(glm::mat3(Camera.getViewMatrix()));

		//Begin writing to another frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, mainFrameBuffer.FBO);

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		//Create two planes to be our stencil buffer mask
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 0, 0xFF);
		glStencilMask(0xFF);

		mapShaderToName["simpleProgram"]->useProgram();
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(1.5f, 0.0f, 1.0f));
		mapShaderToName["simpleProgram"]->setMat4("projection", projection);
		mapShaderToName["simpleProgram"]->setMat4("view", view);
		mapShaderToName["simpleProgram"]->setMat4("model", model);
		glBindVertexArray(planeObject.VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-1.5f, 0.0f, 1.0f));
		mapShaderToName["simpleProgram"]->setMat4("model", model);
		glBindVertexArray(planeObject.VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		//Draw the rest of the objects in the main scene, as well as the skybox
		glStencilFunc(GL_ALWAYS, 1, 0xFF);

		//Draw one quad to place the first texture we made on
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 1.0f));
		mapShaderToName["simpleProgram"]->setMat4("model", model);
		glBindTexture(GL_TEXTURE_2D, miniFrameBuffer.textureID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
		glBindVertexArray(planeObject.VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		//A surprise guest, the Chicago Bean
		mapShaderToName["mainProgram"]->useProgram();
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-5.0f, 0.0f, -10.0f));
		model = glm::scale(model, glm::vec3(1.0f));
		mapShaderToName["mainProgram"]->setMat4("projection", projection);
		mapShaderToName["mainProgram"]->setMat4("view", view);
		mapShaderToName["mainProgram"]->setMat4("model", model);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 2);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
		mapModelsToNames["Bean"]->Draw(*mapShaderToName["mainProgram"]);

		//And another bean
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(5.0f, 0.0f, -10.0f));
		model = glm::scale(model, glm::vec3(1.0f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
		mapModelsToNames["Bean"]->Draw(*mapShaderToName["mainProgram"]);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 0);
		
		//Drawing the main scene's skybox
		mapShaderToName["skyboxProgram"]->useProgram();

		glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
		mapShaderToName["skyboxProgram"]->setMat4("projection", projection);
		mapShaderToName["skyboxProgram"]->setMat4("view", skyboxView);

		glBindVertexArray(skybox.VAO);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.textureID);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glDepthFunc(GL_LESS); // set depth function back to default

		//Render the scene to appear in the stencil buffer
		glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
		glStencilMask(0x00);
		glClear(GL_DEPTH_BUFFER_BIT);

		mapShaderToName["mainProgram"]->useProgram();

		glm::mat4 model(1.0f);
		model = glm::translate(model, glm::vec3(1.0f, 0.0f, -7.0f));
		model = glm::scale(model, glm::vec3(0.4f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		mapModelsToNames["Banana"]->Draw(*mapShaderToName["mainProgram"]);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(1.0f, 0.0f, -8.0f));
		model = glm::scale(model, glm::vec3(0.012f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		mapModelsToNames["Apple"]->Draw(*mapShaderToName["mainProgram"]);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.4f, -9.0f));
		model = glm::scale(model, glm::vec3(0.01f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		mapModelsToNames["Pumpkin"]->Draw(*mapShaderToName["mainProgram"]);

		//Two more Beans, this time they're in space though, and I switched the reflect and refract around
		mapShaderToName["mainProgram"]->useProgram();
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-5.0f, 0.0f, -10.0f));
		model = glm::scale(model, glm::vec3(1.0f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 2);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		mapModelsToNames["Bean"]->Draw(*mapShaderToName["mainProgram"]);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(5.0f, 0.0f, -10.0f));
		model = glm::scale(model, glm::vec3(1.0f));
		mapShaderToName["mainProgram"]->setMat4("model", model);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 2);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		mapModelsToNames["Bean"]->Draw(*mapShaderToName["mainProgram"]);
		mapShaderToName["mainProgram"]->setInt("reflectRefract", 0);

		//Drawing the skybox for the stencil scene
		mapShaderToName["skyboxProgram"]->useProgram();

		glDepthFunc(GL_LEQUAL);
		glBindVertexArray(skybox.VAO);
		glBindTexture(GL_TEXTURE_CUBE_MAP, spacebox.textureID);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glDepthFunc(GL_LESS);
		glStencilMask(0xFF);

		//Clear out the stencil buffer
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilMask(0x00);

		//Final pass: Render all of the above on one quad
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_DEPTH_TEST);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		//Paste the entire scene onto a quad as a single texture
		mapShaderToName["quadProgram"]->useProgram();
		mapShaderToName["quadProgram"]->setInt("drawType", drawType);
		glBindVertexArray(screenQuad.VAO);
		glBindTexture(GL_TEXTURE_2D, mainFrameBuffer.textureID);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	SCR_HEIGHT = height;
	SCR_WIDTH = width;
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	return;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
		drawType = 1;
	if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
		drawType = 2;
	if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
		drawType = 3;
	if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
		drawType = 4;
	if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
		drawType = 5;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		Camera.processKeyboard(Camera_Movement::FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		Camera.processKeyboard(Camera_Movement::BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		Camera.processKeyboard(Camera_Movement::LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		Camera.processKeyboard(Camera_Movement::RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse) // this bool variable is initially set to true
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xOffset = xpos - lastX;
	float yOffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	Camera.processMouseMovement(xOffset, yOffset, true);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Camera.processMouseScroll(yoffset);
}

unsigned int loadCubeMap(std::string directory, std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		std::string fullPath = directory + faces[i];
		unsigned char *data = SOIL_load_image(fullPath.c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			GLenum format;
			if (nrChannels == 1)
				format = GL_RED;
			else if (nrChannels == 3)
				format = GL_RGB;
			else if (nrChannels == 4)
				format = GL_RGBA;

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data
			);
			SOIL_free_image_data(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			SOIL_free_image_data(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}