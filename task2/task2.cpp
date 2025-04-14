#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Define STB_IMAGE_IMPLEMENTATION in one C++ file (here) before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
GLuint compileShader(GLenum type, const char* source);
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);

// --- Settings ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- Shader Source Code (GLSL) ---

// Vertex Shader: Handles vertex positions and texture coordinates
const char *vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord; // Pass texture coordinate to fragment shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord; // Pass through the texture coordinate
}
)glsl";

// Fragment Shader: Samples the texture
const char *fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord; // Receive texture coordinate from vertex shader

uniform sampler2D texture1; // Texture sampler

void main()
{
    // Sample the texture at the interpolated coordinate
    FragColor = texture(texture1, TexCoord);
    // For fun: maybe mix with a solid color based on coordinate?
    // FragColor = texture(texture1, TexCoord) * vec4(TexCoord.x, TexCoord.y, 1.0, 1.0);
}
)glsl";


int main() {
    // 1. Initialize GLFW
    // -------------------
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
#endif

    // 2. Create Window
    // ----------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Task 2: Textured Pyramid", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. Initialize GLAD
    // ------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 4. Configure Global OpenGL State
    // --------------------------------
    glEnable(GL_DEPTH_TEST); // Enable depth testing for 3D

    // 5. Build and Compile Shaders
    // ----------------------------
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = linkProgram(vertexShader, fragmentShader);
    // Delete shaders after linking as they are no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 6. Set up Vertex Data and Buffers
    // ---------------------------------
    // Define vertices for a pyramid (5 points: 1 apex, 4 base)
    // Then define triangles using these points, including texture coordinates
    // We define 6 triangles (4 sides, 2 for the base) - 18 vertices total
    // Position (x, y, z)   Texture Coordinates (s, t)
    float vertices[] = {
        // Sides (4 triangles) - Tip points upwards
        // Side 1 (Front)
         0.0f,  0.5f,  0.0f,  0.5f, 1.0f, // Apex
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // Base Front Left
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, // Base Front Right
        // Side 2 (Right)
         0.0f,  0.5f,  0.0f,  0.5f, 1.0f, // Apex
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // Base Front Right
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // Base Back Right
        // Side 3 (Back)
         0.0f,  0.5f,  0.0f,  0.5f, 1.0f, // Apex
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // Base Back Right
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // Base Back Left
        // Side 4 (Left)
         0.0f,  0.5f,  0.0f,  0.5f, 1.0f, // Apex
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // Base Back Left
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, // Base Front Left

        // Base (2 triangles) - Covers the bottom square
        // Triangle 1
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, // Base Front Left
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, // Base Front Right
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // Base Back Right
        // Triangle 2
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // Base Back Right
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // Base Back Left
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f  // Base Front Left
    };

    // Create Vertex Array Object (VAO), Vertex Buffer Object (VBO)
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // Bind VAO
    glBindVertexArray(VAO);

    // Bind VBO and copy vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Configure Vertex Attributes
    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind VBO (VAO keeps track of this)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Unbind VAO
    glBindVertexArray(0);

    // 7. Load Texture
    // ---------------
    // !! Replace "pyramid_texture.jpg" with the actual path to your texture file !!
    const char* texturePath = "pyramid_texture.jpg"; // Or .png, etc.
    unsigned int texture1 = loadTexture(texturePath);
    if (texture1 == 0) {
        std::cerr << "Failed to load texture: " << texturePath << std::endl;
        // Continue without texture? Or terminate? Let's terminate for now.
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);
        glfwTerminate();
        return -1;
    }

    // Tell OpenGL which texture unit to use for 'texture1' uniform (unit 0)
    glUseProgram(shaderProgram); // Activate shader to set uniform
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    // 8. Rendering Loop
    // -----------------
    while (!glfwWindowShouldClose(window)) {
        // --- Input ---
        processInput(window);

        // --- Rendering ---
        // Set clear color (background)
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f); // Dark blueish background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear color and depth buffers

        // Activate Texture Unit 0 and Bind Texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);

        // Activate Shader Program
        glUseProgram(shaderProgram);

        // Set up transformations (Model, View, Projection)
        glm::mat4 model = glm::mat4(1.0f); // Identity matrix
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);

        // Model: Rotate the pyramid over time for creativity
        float angle = (float)glfwGetTime() * glm::radians(40.0f); // Rotate 40 degrees per second
        model = glm::rotate(model, angle, glm::vec3(0.2f, 1.0f, 0.3f)); // Rotate around a tilted axis

        // View: Move the camera slightly back
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

        // Projection: Perspective projection
        projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // Get matrix uniform locations and set them
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Bind VAO (contains vertex data configuration)
        glBindVertexArray(VAO);

        // Draw the pyramid (18 vertices define 6 triangles)
        glDrawArrays(GL_TRIANGLES, 0, 18);

        // Unbind VAO
        glBindVertexArray(0);

        // --- Swap Buffers and Poll Events ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 9. Cleanup Resources
    // --------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture1);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// --- Helper Function Implementations ---

// GLFW: Adjust viewport on window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// GLFW: Process input (e.g., close window on ESC)
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// Utility function for loading a 2D texture from file
unsigned int loadTexture(char const * path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // Tell stb_image.h to flip loaded texture's on the y-axis (OpenGL expects 0.0 on y-axis to be at the bottom)
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
             std::cerr << "Texture format not supported (nrComponents=" << nrComponents << ") for " << path << std::endl;
             stbi_image_free(data);
             glDeleteTextures(1, &textureID); // Clean up generated texture ID
             return 0; // Indicate failure
        }


        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repeat texture horizontally
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Repeat texture vertically
        // Set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Linear filtering for minification (with mipmaps)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                // Linear filtering for magnification

        stbi_image_free(data); // Free image memory after uploading to GPU
    } else {
        std::cerr << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
        return 0; // Indicate failure
    }

    return textureID;
}


// Utility function to compile a shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader); // Don't leak the shader.
        return 0;
    }
    return shader;
}

// Utility function to link shader program
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteProgram(program); // Don't leak the program.
        return 0;
    }
    return program;
}