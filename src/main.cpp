#define GLFW_DLL

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

//--------------------------------------------------------------------------------------------------------------------------------//
//GLOBALS:

#define WORK_GROUP_SIZE 16

GLuint screenW = 1920;
GLuint screenH = 1080;
bool running = true;

//fractal parameters:
const float CAMSPEED = 1.0f;
const float SCALESPEED = 0.05f;
GLfloat positionX = 0.0f;
GLfloat positionY = 0.0f;
GLfloat scale = 1.0f;
GLuint maxIterations = 1000;
GLuint mandelPower = 2;
GLfloat color[3] = {1.0f, 1.0f, 1.0f};
GLfloat rPower = 0.5f, gPower = 1.0f, bPower = 10.0f;

//--------------------------------------------------------------------------------------------------------------------------------//
//UTILITY FUNCTIONS:

int compile_shader(const char* path, GLenum type);

//--------------------------------------------------------------------------------------------------------------------------------//
//CALLBACK FUNCTIONS:

void GLAPIENTRY gl_message_callback          (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
void            gl_framebuffer_size_callback (GLFWwindow* window, int w, int h);
void            gl_key_callback              (GLFWwindow* window, int key, int scancode, int action, int mods);
void			gl_scroll_callback			 (GLFWwindow* window, double xoffset, double yoffset);

//--------------------------------------------------------------------------------------------------------------------------------//
//MAIN: 

int main()
{
	//init GLFW:
	//---------------------------------
	if(glfwInit() == GL_FALSE)
    {
        printf("FAILED TO INITIALIZE GLFW\n");
        running = false;
    }
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	//create and init window:
	//---------------------------------
	GLFWwindow* window = glfwCreateWindow(screenW, screenH, "Multibrot Viewer", NULL, NULL);
	if(window == NULL)
	{
		printf("FAILED TO CREATE WINDOW\n");
        running = false;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	//load opengl functions:
	//---------------------------------
	if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("Failed to initialize GLAD\n");
		running = false;
	}

	//set gl viewport:
	//---------------------------------
	glViewport(0, 0, screenW, screenH);

	//set callback functions:
	//---------------------------------
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(gl_message_callback, 0);
    glfwSetFramebufferSizeCallback(window, gl_framebuffer_size_callback);
    glfwSetKeyCallback(window, gl_key_callback);
    glfwSetScrollCallback(window, gl_scroll_callback);

    //init dear imgui:
    //---------------------------------
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430");
	ImGui::StyleColorsDark();

	float scaleX, scaleY;
	glfwGetWindowContentScale(window, &scaleX, &scaleY);
	io.FontGlobalScale = scaleX;

    //compile shaders:
    //---------------------------------

	//fractal shader:
	unsigned int fractalShader = compile_shader("shaders/fractals.comp", GL_COMPUTE_SHADER);

	unsigned int fractalProgram = glCreateProgram();
	glAttachShader(fractalProgram, fractalShader);
	glLinkProgram(fractalProgram);

	int success;
	glGetProgramiv(fractalProgram, GL_LINK_STATUS, &success);
	if(!success)
	{
		glDeleteShader(fractalShader);
		glDeleteProgram(fractalProgram);
		printf("ERROR - COULD NOT GENERATE SHADER PROGRAM\n");
		running = false;
	}

	glDeleteShader(fractalShader);

	//quad shader:
	unsigned int quadVert = compile_shader("shaders/quad.vert", GL_VERTEX_SHADER);
	unsigned int quadFrag = compile_shader("shaders/quad.frag", GL_FRAGMENT_SHADER);

	unsigned int quadProgram = glCreateProgram();
	glAttachShader(quadProgram, quadVert);
	glAttachShader(quadProgram, quadFrag);
	glLinkProgram(quadProgram);

	glGetProgramiv(quadProgram, GL_LINK_STATUS, &success);
	if(!success)
	{
		glDeleteShader(quadVert);
		glDeleteShader(quadFrag);
		glDeleteProgram(quadProgram);
		return -1;
	}

	glDeleteShader(quadVert);
	glDeleteShader(quadFrag);

	//generate quad buffer:
	//---------------------------------
	GLfloat quadVertices[] = 
	{
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f
	};
	GLuint quadIndices[] = 
	{
		0, 1, 3,
		1, 2, 3
	};

	unsigned int quadBuffer, VBO, EBO;
	glGenVertexArrays(1, &quadBuffer);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(quadBuffer);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	if(glGetError() == GL_OUT_OF_MEMORY)
	{
		printf("ERROR - FAILED TO GENERATE FINAL QUAD BUFFER\n");
		glDeleteVertexArrays(1, &quadBuffer);
		running = false;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
	if(glGetError() == GL_OUT_OF_MEMORY)
	{
		printf("ERROR - FAILED TO GENERATE FINAL QUAD BUFFER\n");
		glDeleteVertexArrays(1, &quadBuffer);
		running = false;
	}

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(long long)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(long long)(3 * sizeof(GL_FLOAT)));
	glEnableVertexAttribArray(1);

    //generate texture:
    //---------------------------------
	unsigned int finalTexture;
	glGenTextures(1, &finalTexture);
	glBindTexture(GL_TEXTURE_2D, finalTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenW - screenW % WORK_GROUP_SIZE, screenH - screenH % WORK_GROUP_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, finalTexture);
	glBindImageTexture(0, finalTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    //main loop:
    //---------------------------------
	float lastFrame = glfwGetTime();

    while(running)
    {
		//find deltatime:
		float currentTime = glfwGetTime();
		float deltaTime = currentTime - lastFrame;
		lastFrame = currentTime;

		//update camera:
		if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			positionY += deltaTime * CAMSPEED / scale;
		if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			positionY -= deltaTime * CAMSPEED / scale;
		if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			positionX += deltaTime * CAMSPEED / scale;
		if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			positionX -= deltaTime * CAMSPEED / scale;

		//dispatch compute shader:
		glUseProgram(fractalProgram);
		glUniform1f (glGetUniformLocation(fractalProgram, "positionX"), positionX);
		glUniform1f (glGetUniformLocation(fractalProgram, "positionY"), positionY);
		glUniform1f (glGetUniformLocation(fractalProgram, "scale"), scale);
		glUniform1ui(glGetUniformLocation(fractalProgram, "maxIter"), maxIterations);
		glUniform1ui(glGetUniformLocation(fractalProgram, "mandelPower"), mandelPower);
		glUniform3fv(glGetUniformLocation(fractalProgram, "color"), 1, color);
		glUniform1f (glGetUniformLocation(fractalProgram, "rPower"), rPower);
		glUniform1f (glGetUniformLocation(fractalProgram, "gPower"), gPower);
		glUniform1f (glGetUniformLocation(fractalProgram, "bPower"), bPower);
		glDispatchCompute(screenW / 16, screenH / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		//run final quad shader:
		glUseProgram(quadProgram);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(0 * sizeof(unsigned int)));

		//render gui:
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Settings");

		ImGui::Text("Scale: %f", scale);
		ImGui::Text("Position Offset: (%f, %f)", positionX, positionY);
		ImGui::Text("Frametime: %fms", 1000 * deltaTime);

		ImGui::Text("---------------------------------------------------");

		ImGui::SliderInt("Maximum Iterations", (int*)&maxIterations, 2, 10000, "%d", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderInt("Z power", (int*)&mandelPower, 2, 50);
		mandelPower -= mandelPower % 2;

		ImGui::Text("---------------------------------------------------");

		ImGui::SliderFloat("R-channel Power", &rPower, 0.0f, 10.0f, "%f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("G-channel Power", &gPower, 0.0f, 10.0f, "%f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("B-channel Power", &bPower, 0.0f, 10.0f, "%f", ImGuiSliderFlags_Logarithmic);
		ImGui::ColorPicker3("Color", color, ImGuiColorEditFlags_InputRGB);

        ImGui::End();
		ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		//swap buffers and poll events:
        glfwSwapBuffers(window);
        glfwPollEvents();

        if(glfwWindowShouldClose(window))
            running = false;
    }

	//clean up and return:
	glDeleteProgram(quadProgram);
	glDeleteProgram(fractalProgram);
    glfwTerminate();

    return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------//

int compile_shader(const char* path, GLenum type)
{
    //load into a buffer:
    //---------------------------------
    char* source = 0;
	long length;
	FILE* file = fopen(path, "rb");

	if(file)
	{
		bool fileSuccess = false;

		fseek(file, 0, SEEK_END);
		length = ftell(file);
		fseek(file, 0, SEEK_SET);
		source = (char*)malloc(length + 1);

		if(source)
		{
			if(fread(source, length, 1, file) == 1)
			{
				source[length] = '\0';
				fileSuccess = true;
			}
			else
			{
				printf("ERROR - COULD NOT READ FROM FILE %s\n", path);
				fileSuccess = false;
				free(source);
			}
		}
		else
		{
			printf("ERROR - COULD NOT ALLOCATE MEMORY FOR SHADER SOURCE CODE");
			fileSuccess = false;
		}

		fclose(file);
		if(!fileSuccess)
            return -1;
	}
	else
	{
		printf("ERROR - COULD NOT OPEN FILE %s\n", path);
		return -1;
	}

    //compile:
    //---------------------------------
	unsigned int shader;
	int success;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	free(source);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success)
	{
		GLsizei logLength;
		char message[1024];
		glGetShaderInfoLog(shader, 1024, &logLength, message);
		printf("\n\nINFO LOG - %s%s\n\n\n", message, path);

		glDeleteShader(shader);
		return -1;
	}

	return shader;
}

//--------------------------------------------------------------------------------------------------------------------------------//

void GLAPIENTRY gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	printf("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
          (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
           type, severity, message);
}

void gl_framebuffer_size_callback(GLFWwindow* window, int w, int h)
{
	screenW = w;
	screenH = h;
    glViewport(0, 0, screenW, screenH);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenW - screenW % WORK_GROUP_SIZE, screenH - screenH % WORK_GROUP_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

void gl_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)  
        running = false;
}

void gl_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	scale += yoffset * SCALESPEED * scale;
}