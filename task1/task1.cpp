#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -- 着色器源码 --

// Simple Lighting / Gouraud Shading Vertex Shader
const char* simpleGouraudVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 LightingColor; // 输出到片段着色器的颜色

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 lightPos;    // 光源位置 (世界空间)
uniform vec3 viewPos;     // 观察者位置 (世界空间)
uniform vec3 lightColor;  // 光源颜色
uniform vec3 objectColor; // 物体基础颜色

void main()
{
    // 转换到世界空间
    vec3 FragPos = vec3(model * vec4(aPos, 1.0));
    vec3 Normal = mat3(transpose(inverse(model))) * aNormal; // 法线转换

    // 环境光
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // 漫反射光
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 镜面反射光
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32); // 32 是高光度
    vec3 specular = specularStrength * spec * lightColor;

    LightingColor = (ambient + diffuse + specular) * objectColor;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Simple Lighting / Gouraud Shading Fragment Shader
const char* simpleGouraudFragmentShaderSource = R"(
#version 330 core
in vec3 LightingColor; // 从顶点着色器接收的插值颜色

out vec4 FragColor;

void main()
{
    FragColor = vec4(LightingColor, 1.0);
}
)";

// Phong Shading Vertex Shader
const char* phongVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;  // 输出到片段着色器的世界空间位置
out vec3 Normal;   // 输出到片段着色器的世界空间法线

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    // 注意法线转换：使用逆转置矩阵以处理非均匀缩放
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Phong Shading Fragment Shader
const char* phongFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos; // 从顶点着色器接收的插值片段位置
in vec3 Normal;  // 从顶点着色器接收的插值法线

uniform vec3 lightPos;    // 光源位置 (世界空间)
uniform vec3 viewPos;     // 观察者位置 (世界空间)
uniform vec3 lightColor;  // 光源颜色
uniform vec3 objectColor; // 物体基础颜色

void main()
{
    // 环境光
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // 漫反射光
    vec3 norm = normalize(Normal); // 需要重新标准化插值后的法线
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 镜面反射光
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64); // Phong通常用更高的高光度
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
)";


// -- 函数声明 --
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int compileShader(const char* source, GLenum type); // 编译单个着色器
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource); // 使用源码创建程序
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices, float radius, int sectorCount, int stackCount);

// -- 全局变量/常量 --
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// 光照参数
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
// 观察者位置在渲染循环中根据view矩阵动态确定或设置

// 球体颜色
glm::vec3 sphereColors[] = {
    glm::vec3(1.0f, 0.5f, 0.31f), // 橙色 (Simple/Gouraud)
    glm::vec3(0.5f, 1.0f, 0.5f),  // 绿色 (Gouraud - 仅用于区分窗口)
    glm::vec3(0.5f, 0.6f, 1.0f)   // 蓝色 (Phong)
};

// 结构体用于存储每个窗口及其相关数据
struct WindowData {
    GLFWwindow* window = nullptr;
    unsigned int shaderProgram = 0;
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    size_t indexCount = 0;
    glm::vec3 objectColor;
    std::string title;
    bool shouldClose = false;
};

// -- main 函数 --
int main()
{
    // 1. 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 2. 创建窗口和数据结构
    std::map<GLFWwindow*, WindowData> windows;
    const char* titles[] = {"Simple/Vertex Lighting", "Gouraud Shading (Same as Vertex)", "Phong Shading"};
    // 注意：前两个使用相同的着色器源码
    const char* vertexShaders[] = {simpleGouraudVertexShaderSource, simpleGouraudVertexShaderSource, phongVertexShaderSource};
    const char* fragmentShaders[] = {simpleGouraudFragmentShaderSource, simpleGouraudFragmentShaderSource, phongFragmentShaderSource};

    for (int i = 0; i < 3; ++i) {
        GLFWwindow* glfwWindow = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, titles[i], NULL, NULL);
        if (glfwWindow == NULL) {
            std::cerr << "Failed to create GLFW window for " << titles[i] << std::endl;
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(glfwWindow); // 重要：在加载GLAD前设置当前上下文
        glfwSetFramebufferSizeCallback(glfwWindow, framebuffer_size_callback);

        // 3. 初始化 GLAD (需要在创建第一个窗口并设置上下文后)
        if (i == 0) { // 只需要加载一次
             if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cerr << "Failed to initialize GLAD" << std::endl;
                glfwTerminate();
                return -1;
             }
        }

        WindowData data;
        data.window = glfwWindow;
        data.title = titles[i];
        data.objectColor = sphereColors[i];

        // 4. 构建和编译着色器程序 (使用源码字符串)
        data.shaderProgram = createShaderProgram(vertexShaders[i], fragmentShaders[i]);
        if (data.shaderProgram == 0) {
             glfwTerminate();
             return -1; // Shader creation failed
        }

        // 5. 设置顶点数据和缓冲区
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        generateSphere(vertices, indices, 1.0f, 36, 18); // 半径1.0, 36x18段
        data.indexCount = indices.size();

        glGenVertexArrays(1, &data.VAO);
        glGenBuffers(1, &data.VBO);
        glGenBuffers(1, &data.EBO);

        glBindVertexArray(data.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, data.VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // 位置属性 (每个顶点6个float: pos.x, pos.y, pos.z, norm.x, norm.y, norm.z)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // 法线属性
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑VBO
        glBindVertexArray(0);             // 解绑VAO

        // 开启深度测试
        glEnable(GL_DEPTH_TEST);

        windows[glfwWindow] = data; // 存储窗口数据
    }

    // 6. 渲染循环
    while (!windows.empty()) // 当还有窗口存在时继续
    {
        glfwPollEvents(); // 检查事件

        auto it = windows.begin();
        while (it != windows.end()) {
            GLFWwindow* currentWindow = it->first;
            WindowData& data = it->second;

            if (glfwWindowShouldClose(currentWindow)) {
                data.shouldClose = true;
                it++; // 继续检查下一个
                continue; // 不渲染已标记关闭的窗口
            }

            // 使当前窗口的上下文成为当前
            glfwMakeContextCurrent(currentWindow);

            // 处理输入 (特定于当前窗口)
            processInput(currentWindow);

            // --- 渲染 ---
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 激活着色器
            glUseProgram(data.shaderProgram);

            // 设置光照和物体颜色 Uniforms
            glUniform3fv(glGetUniformLocation(data.shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
            glUniform3fv(glGetUniformLocation(data.shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
            glUniform3fv(glGetUniformLocation(data.shaderProgram, "objectColor"), 1, glm::value_ptr(data.objectColor));


            // 创建变换矩阵
            glm::mat4 model = glm::mat4(1.0f);
            // 让球体旋转以更好地观察光照效果
            model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));

            glm::mat4 view = glm::mat4(1.0f);
            // 将摄像机向后移动一点
            glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5.0f); // 摄像机位置
            view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // 获取当前窗口尺寸用于投影矩阵 (虽然我们用了常量，但更健壮的做法是查询)
            int currentWidth, currentHeight;
            glfwGetFramebufferSize(currentWindow, &currentWidth, &currentHeight);
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)currentWidth / (float)currentHeight, 0.1f, 100.0f);

            // 设置观察者位置 Uniform (用于镜面反射计算)
            glUniform3fv(glGetUniformLocation(data.shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));


            // 将矩阵传递给着色器
            glUniformMatrix4fv(glGetUniformLocation(data.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(data.shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(data.shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            // 绘制球体
            glBindVertexArray(data.VAO);
            glDrawElements(GL_TRIANGLES, data.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0); // 解绑

            // 交换缓冲区
            glfwSwapBuffers(currentWindow);

            it++; // 处理下一个窗口
        }

        // 清理需要关闭的窗口
         it = windows.begin();
        while (it != windows.end()) {
             if (it->second.shouldClose) {
                 // 清理OpenGL资源 (Make context current first might be safer, though often not strictly required for deletion)
                 // glfwMakeContextCurrent(it->first); // 可选，但有时有助于避免跨上下文问题
                 glDeleteVertexArrays(1, &it->second.VAO);
                 glDeleteBuffers(1, &it->second.VBO);
                 glDeleteBuffers(1, &it->second.EBO);
                 glDeleteProgram(it->second.shaderProgram);
                 // 销毁窗口
                 glfwDestroyWindow(it->first);
                 // 从map中移除
                 it = windows.erase(it); // erase返回下一个有效迭代器
             } else {
                 it++;
             }
         }
    }


    // 7. 清理 GLFW 资源 (当所有窗口关闭后)
    glfwTerminate();
    return 0;
}

// -- 辅助函数实现 --

// 处理输入: 按下ESC键关闭当前窗口
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// GLFW: 窗口大小改变时，回调函数设置视口
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // 确保回调发生时，正确的上下文是当前的 (虽然GLFW通常会处理)
    // glfwMakeContextCurrent(window); // 可选，增加健壮性
    glViewport(0, 0, width, height);
}

// 编译着色器 (从源码字符串)
unsigned int compileShader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // 检查编译错误
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        const char* shaderTypeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::cerr << "ERROR::SHADER::" << shaderTypeStr << "::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader); // 删除失败的着色器
        return 0;
    }
    return shader;
}

// 创建着色器程序 (从源码字符串)
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // 编译着色器
    unsigned int vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    if (vertexShader == 0) return 0;
    unsigned int fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
     if (fragmentShader == 0) {
        glDeleteShader(vertexShader); // 清理已创建的顶点着色器
         return 0;
    }

    // 创建着色器程序
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // 检查链接错误
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
         glDeleteProgram(shaderProgram); // 删除失败的程序
         shaderProgram = 0; // 返回0表示失败
    }

    // 删除不再需要的着色器对象 (链接后不再需要)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}


// 生成球体顶点数据 (位置和法线交错) 和索引
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices, float radius, int sectorCount, int stackCount) {
    vertices.clear();
    indices.clear();

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal

    float sectorStep = 2.0f * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    // 预留空间以提高效率 (可选)
    vertices.reserve((stackCount + 1) * (sectorCount + 1) * 6);
    indices.reserve(stackCount * sectorCount * 6);


    for(int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2.0f - i * stackStep;      // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but may have different tex coords in other scenarios
        for(int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    // generate index list for triangles
    // k1--k1+1
    // |  / |
    // k2--k2+1
    unsigned int k1, k2;
    for(int i = 0; i < stackCount; ++i) {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            // 2 triangles per sector, formed correctly for GL_TRIANGLES

            // first triangle of sector: (k1, k2, k1+1)
            if (i != 0) { // Aovid creating triangles at the top pole degenerating into lines
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

             // second triangle of sector: (k1+1, k2, k2+1)
            if (i != (stackCount - 1)) { // Avoid creating triangles at the bottom pole degenerating into lines
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}