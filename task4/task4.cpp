#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 函数声明
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int compileShader(unsigned int type, const char* source);
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
std::vector<float> createSphere(float radius, int sectorCount, int stackCount);
void drawOrbit(unsigned int shaderProgram, float radius, const glm::mat4& view, const glm::mat4& projection, float tiltAngle = 0.0f, const glm::vec3& tiltAxis = glm::vec3(1.0f, 0.0f, 0.0f));
void drawRing(unsigned int shaderProgram, float innerRadius, float outerRadius, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& planetModelMatrix, float tiltAngle, const glm::vec3& tiltAxis);


// 窗口设置
const unsigned int SCR_WIDTH = 1200; // 增加窗口宽度以便更好地显示
const unsigned int SCR_HEIGHT = 800; // 增加窗口高度

// 顶点着色器源码 (GLSL)
const char *vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos; // 顶点位置输入

    uniform mat4 model;      // 模型矩阵 (物体局部坐标 -> 世界坐标)
    uniform mat4 view;       // 视图矩阵 (世界坐标 -> 观察空间)
    uniform mat4 projection; // 投影矩阵 (观察空间 -> 裁剪空间)

    void main()
    {
        // 将顶点位置通过 MVP 矩阵变换到裁剪空间
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

// 片段着色器源码 (GLSL)
const char *fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor; // 输出的颜色

    uniform vec3 objectColor; // 从 CPU 传入的物体颜色

    void main()
    {
        // 直接使用传入的颜色作为片段的最终颜色
        FragColor = vec4(objectColor, 1.0f);
    }
)";

// 创建球体顶点数据
std::vector<float> createSphere(float radius, int sectorCount, int stackCount) {
    std::vector<float> vertices;
    float x, y, z, xy;
    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    for(int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);
        for(int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    return vertices;
}

// 绘制轨道
void drawOrbit(unsigned int shaderProgram, float radius, const glm::mat4& view, const glm::mat4& projection, float tiltAngle, const glm::vec3& tiltAxis) {
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(colorLoc, 0.3f, 0.3f, 0.3f);  // 轨道颜色调暗一些

    glm::mat4 model = glm::mat4(1.0f);
    if (tiltAngle != 0.0f) {
        model = glm::rotate(model, tiltAngle, tiltAxis);
    }
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    const int segments = 100;
    std::vector<float> orbitVertices;
    for (int i = 0; i <= segments; ++i) { // Use <= to close the loop
        float theta = 2.0f * M_PI * float(i) / float(segments);
        float x = radius * cosf(theta);
        float z = radius * sinf(theta); // Orbits are generally in XZ plane relative to the sun
        orbitVertices.push_back(x);
        orbitVertices.push_back(0.0f);
        orbitVertices.push_back(z);
    }

    unsigned int orbitVAO, orbitVBO;
    glGenVertexArrays(1, &orbitVAO);
    glGenBuffers(1, &orbitVBO);
    glBindVertexArray(orbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVertices.size() * sizeof(float), orbitVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINE_STRIP, 0, segments + 1); // Use GL_LINE_STRIP for non-closed loop if segments is not +1

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &orbitVBO);
    glDeleteVertexArrays(1, &orbitVAO);
}

// 绘制土星环
void drawRing(unsigned int shaderProgram, float innerRadius, float outerRadius, 
              const glm::mat4& view, const glm::mat4& projection, 
              const glm::mat4& planetWorldMatrix, // Pass the planet's full world matrix
              float tiltAngle, const glm::vec3& tiltAxis) {

    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(colorLoc, 0.6f, 0.6f, 0.5f); // 环的颜色 (淡黄色)

    glm::mat4 ringModel = planetWorldMatrix; // Start with the planet's transformation
    // The ring itself is flat, its orientation comes from the planet's tilt
    // If the planet has an axial tilt, the ring should align with its equator
    // The planetModelMatrix already includes orbital position and axial tilt.
    // We just need to scale and draw the ring in the planet's XY plane.

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(ringModel));


    const int segments = 72; // 环的段数
    std::vector<float> ringVertices;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * float(i) / float(segments);
        // 外环顶点
        ringVertices.push_back(outerRadius * cosf(angle));
        ringVertices.push_back(outerRadius * sinf(angle));
        ringVertices.push_back(0.0f); // 环在XY平面上
        // 内环顶点
        ringVertices.push_back(innerRadius * cosf(angle));
        ringVertices.push_back(innerRadius * sinf(angle));
        ringVertices.push_back(0.0f);
    }

    unsigned int ringVAO, ringVBO;
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, ringVertices.size() * sizeof(float), ringVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segments + 1) * 2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &ringVBO);
    glDeleteVertexArrays(1, &ringVAO);
}


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

    // 2. 创建 GLFW 窗口
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "太阳系模拟", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 4. 构建和编译着色器程序
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // 5. 设置顶点数据和缓冲区 (为单位球体，实际大小通过model矩阵控制)
    std::vector<float> sphereVertices = createSphere(1.0f, 36, 18); // 单位球体
    
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f); // 设置轨道线宽

    // 6. 渲染循环
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.01f, 0.01f, 0.02f, 1.0f); // 更深的太空背景
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f); // 增加 far plane
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // 调整摄像机位置以容纳更大的太阳系
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 30.0f, 60.0f), // 摄像机位置 (更高更远)
                                     glm::vec3(0.0f, 0.0f, 0.0f),  // 目标位置
                                     glm::vec3(0.0f, 1.0f, 0.0f)); // 上向量
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        float timeValue = (float)glfwGetTime();

        // 行星参数 (半径单位：任意，轨道半径单位：任意，速度：相对值)
        // 太阳
        float sunRadius = 2.5f;
        float sunRotationSpeed = 0.05f;

        // 水星 (Mercury)
        float mercuryOrbitRadius = 5.0f;
        float mercuryRadius = 0.2f;
        float mercuryOrbitalSpeed = 1.0f * 0.7f; // 相对地球更快
        float mercuryRotationSpeed = 0.1f;
        glm::vec3 mercuryColor = glm::vec3(0.6f, 0.6f, 0.6f); // 灰色

        // 金星 (Venus)
        float venusOrbitRadius = 8.0f;
        float venusRadius = 0.5f;
        float venusOrbitalSpeed = 0.7f * 0.7f;
        float venusRotationSpeed = -0.05f; // 缓慢逆行自转
        float venusAxialTilt = glm::radians(177.0f); // 大倾角
        glm::vec3 venusColor = glm::vec3(0.9f, 0.85f, 0.7f); // 黄白色

        // 地球 (Earth)
        float earthOrbitRadius = 12.0f;
        float earthRadius = 0.6f;
        float earthOrbitalSpeed = 0.5f * 0.7f;
        float earthRotationSpeed = 1.0f;
        float earthAxialTilt = glm::radians(23.5f);
        glm::vec3 earthColor = glm::vec3(0.2f, 0.4f, 0.8f); // 蓝色

        // 月球 (Moon)
        float moonOrbitRadius = 1.2f; // 相对地球
        float moonRadius = 0.15f;
        float moonOrbitalSpeed = 2.5f; // 相对地球公转
        glm::vec3 moonColor = glm::vec3(0.7f, 0.7f, 0.7f); // 浅灰色

        // 火星 (Mars)
        float marsOrbitRadius = 17.0f;
        float marsRadius = 0.35f;
        float marsOrbitalSpeed = 0.35f * 0.7f;
        float marsRotationSpeed = 0.9f;
        float marsAxialTilt = glm::radians(25.0f);
        glm::vec3 marsColor = glm::vec3(0.8f, 0.3f, 0.1f); // 红色

        // 木星 (Jupiter)
        float jupiterOrbitRadius = 25.0f;
        float jupiterRadius = 1.5f; // 最大行星
        float jupiterOrbitalSpeed = 0.15f * 0.7f;
        float jupiterRotationSpeed = 2.2f; // 快速自转
        float jupiterAxialTilt = glm::radians(3.0f);
        glm::vec3 jupiterColor = glm::vec3(0.8f, 0.7f, 0.5f); // 橙棕色

        // 土星 (Saturn)
        float saturnOrbitRadius = 35.0f;
        float saturnRadius = 1.2f;
        float saturnOrbitalSpeed = 0.1f * 0.7f;
        float saturnRotationSpeed = 1.9f;
        float saturnAxialTilt = glm::radians(27.0f); // 轴倾角
        float saturnOrbitalTilt = glm::radians(2.5f); // 轨道倾角 (相对黄道面)
        glm::vec3 saturnColor = glm::vec3(0.9f, 0.8f, 0.6f); // 淡黄色
        float saturnRingInnerRadius = saturnRadius * 1.2f;
        float saturnRingOuterRadius = saturnRadius * 2.2f;


        // 绘制所有轨道 (除了太阳)
        drawOrbit(shaderProgram, mercuryOrbitRadius, view, projection);
        drawOrbit(shaderProgram, venusOrbitRadius, view, projection);
        drawOrbit(shaderProgram, earthOrbitRadius, view, projection);
        drawOrbit(shaderProgram, marsOrbitRadius, view, projection);
        drawOrbit(shaderProgram, jupiterOrbitRadius, view, projection);
        // 土星轨道需要考虑其轨道倾角
        drawOrbit(shaderProgram, saturnOrbitRadius, view, projection, saturnOrbitalTilt, glm::vec3(1.0f, 0.0f, 0.0f));


        glBindVertexArray(VAO); // 绑定一次球体VAO，用于所有球形天体

        // --- 绘制太阳 ---
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, timeValue * sunRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 太阳自转
        model = glm::scale(model, glm::vec3(sunRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(colorLoc, 1.0f, 0.8f, 0.0f); // 太阳颜色
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制水星 ---
        glm::mat4 mercuryModel = glm::mat4(1.0f);
        mercuryModel = glm::rotate(mercuryModel, timeValue * mercuryOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 公转
        mercuryModel = glm::translate(mercuryModel, glm::vec3(mercuryOrbitRadius, 0.0f, 0.0f));
        mercuryModel = glm::rotate(mercuryModel, timeValue * mercuryRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 自转
        mercuryModel = glm::scale(mercuryModel, glm::vec3(mercuryRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mercuryModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(mercuryColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制金星 ---
        glm::mat4 venusModel = glm::mat4(1.0f);
        venusModel = glm::rotate(venusModel, timeValue * venusOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 公转
        venusModel = glm::translate(venusModel, glm::vec3(venusOrbitRadius, 0.0f, 0.0f));
        venusModel = glm::rotate(venusModel, venusAxialTilt, glm::vec3(0.0f, 0.0f, 1.0f)); // 轴倾角
        venusModel = glm::rotate(venusModel, timeValue * venusRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 自转
        venusModel = glm::scale(venusModel, glm::vec3(venusRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(venusModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(venusColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制地球 ---
        glm::mat4 earthModel = glm::mat4(1.0f);
        earthModel = glm::rotate(earthModel, timeValue * earthOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 公转
        earthModel = glm::translate(earthModel, glm::vec3(earthOrbitRadius, 0.0f, 0.0f));
        // 保存地球的世界坐标变换，用于月球
        glm::mat4 earthWorldModel = earthModel; 
        earthModel = glm::rotate(earthModel, earthAxialTilt, glm::vec3(0.0f, 0.0f, 1.0f)); // 地球轴倾角 (先倾斜再自转)
        earthModel = glm::rotate(earthModel, timeValue * earthRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 地球自转
        earthModel = glm::scale(earthModel, glm::vec3(earthRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(earthModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(earthColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制月球 ---
        glm::mat4 moonModel = earthWorldModel; // 从地球的世界变换开始
        moonModel = glm::rotate(moonModel, timeValue * moonOrbitalSpeed, glm::vec3(0.1f, 1.0f, 0.1f)); // 月球公转 (可以稍微倾斜轨道)
        moonModel = glm::translate(moonModel, glm::vec3(moonOrbitRadius, 0.0f, 0.0f));
        // 月球通常是潮汐锁定的，可以不加独立自转或使其与公转同步
        moonModel = glm::scale(moonModel, glm::vec3(moonRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(moonModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(moonColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制火星 ---
        glm::mat4 marsModel = glm::mat4(1.0f);
        marsModel = glm::rotate(marsModel, timeValue * marsOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
        marsModel = glm::translate(marsModel, glm::vec3(marsOrbitRadius, 0.0f, 0.0f));
        marsModel = glm::rotate(marsModel, marsAxialTilt, glm::vec3(0.0f, 0.0f, 1.0f));
        marsModel = glm::rotate(marsModel, timeValue * marsRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
        marsModel = glm::scale(marsModel, glm::vec3(marsRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(marsModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(marsColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制木星 ---
        glm::mat4 jupiterModel = glm::mat4(1.0f);
        jupiterModel = glm::rotate(jupiterModel, timeValue * jupiterOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
        jupiterModel = glm::translate(jupiterModel, glm::vec3(jupiterOrbitRadius, 0.0f, 0.0f));
        jupiterModel = glm::rotate(jupiterModel, jupiterAxialTilt, glm::vec3(0.0f, 0.0f, 1.0f));
        jupiterModel = glm::rotate(jupiterModel, timeValue * jupiterRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
        jupiterModel = glm::scale(jupiterModel, glm::vec3(jupiterRadius));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(jupiterModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(jupiterColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // --- 绘制土星 ---
        glm::mat4 saturnModel = glm::mat4(1.0f);
        // 1. 轨道倾斜
        saturnModel = glm::rotate(saturnModel, saturnOrbitalTilt, glm::vec3(1.0f, 0.0f, 0.0f)); 
        // 2. 公转
        saturnModel = glm::rotate(saturnModel, timeValue * saturnOrbitalSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 绕Y轴公转
        // 3. 平移到轨道半径
        saturnModel = glm::translate(saturnModel, glm::vec3(saturnOrbitRadius, 0.0f, 0.0f));
        
        glm::mat4 saturnWorldModel = saturnModel; // 保存土星不带自转和轴倾斜的变换，用于环

        // 4. 轴倾角 (影响自转轴和环的平面)
        // 重要：轴倾角应相对于轨道平面。如果轨道本身倾斜了，这个旋转轴也应该相应调整。
        // 简单起见，我们假设轴倾角是相对于其局部坐标系的Y轴定义的，然后应用到世界变换后的模型上。
        // 或者，更准确地，先应用轨道变换，然后应用轴倾角，再自转。
        // 这里的tiltAxis应该是垂直于轨道平面的。对于未倾斜轨道，是(0,0,1)或(1,0,0)。
        // 对于倾斜轨道，这个轴本身也需要被轨道倾斜变换。
        // 为了简化，我们将轴倾角旋转应用于已经定位到轨道上的行星。
        // 旋转轴 (e.g., Z-axis for tilt, then Y-axis for rotation)
        // The tilt should be around an axis perpendicular to its orbital motion AND its 'up' vector.
        // A common way is to tilt around its local X or Z axis before self-rotation around Y.
        saturnModel = glm::rotate(saturnModel, saturnAxialTilt, glm::vec3(cos(timeValue * saturnOrbitalSpeed + M_PI/2.0), 0.0f, sin(timeValue * saturnOrbitalSpeed + M_PI/2.0))); // 倾斜自转轴
                                                                                                                                                                         // More simply: tilt around a fixed axis like Z if orbit is in XY plane
                                                                                                                                                                         // For orbit in XZ plane, tilt around X or Z.
        // Let's use a consistent tilt axis (e.g. local Z axis of the planet AFTER orbital positioning but BEFORE self-rotation)
        // To make the tilt appear consistent with the orbit, the tilt axis should be chosen carefully.
        // If orbit is around global Y, and planet moves in XZ plane, tilt can be around local X or Z.
        // Let's tilt its "spin axis pole" towards/away from the direction of motion or perpendicular to it.
        // A common convention for axial tilt is rotation around an axis like (1,0,0) or (0,0,1) *before* self-rotation.
        // The saturnWorldModel already has orbital placement and orbital tilt.
        // Now apply axial tilt relative to that.
        
        glm::mat4 saturnPlanetPart = saturnWorldModel; // Start from here for the planet itself
        saturnPlanetPart = glm::rotate(saturnPlanetPart, saturnAxialTilt, glm::vec3(1.0f, 0.0f, 0.0f)); // Tilt around its X-axis
        saturnPlanetPart = glm::rotate(saturnPlanetPart, timeValue * saturnRotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f)); // 自转
        saturnPlanetPart = glm::scale(saturnPlanetPart, glm::vec3(saturnRadius));
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(saturnPlanetPart));
        glUniform3fv(colorLoc, 1, glm::value_ptr(saturnColor));
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // 绘制土星环 - 环应该与土星的赤道面对齐，即受到轴倾角影响
        // The ring's model matrix should be based on saturnWorldModel (position in orbit, orbital tilt)
        // and then apply the *same axial tilt* as Saturn itself.
        glm::mat4 ringBaseModel = saturnWorldModel;
        ringBaseModel = glm::rotate(ringBaseModel, saturnAxialTilt, glm::vec3(1.0f, 0.0f, 0.0f)); // Apply same axial tilt
        // The drawRing function expects the model matrix to position and orient the ring plane.
        // The vertices of the ring are in its local XY plane.
        drawRing(shaderProgram, saturnRingInnerRadius, saturnRingOuterRadius, view, projection, ringBaseModel, 0.0f, glm::vec3(0,0,1)); // No additional tilt for drawRing, it's in ringBaseModel


        glBindVertexArray(0); // 解绑VAO

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 7. 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "错误::着色器::" << (type == GL_VERTEX_SHADER ? "顶点" : "片段") << "::编译失败\n" << infoLog << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) return 0;

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "错误::着色器::程序::链接失败\n" << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

