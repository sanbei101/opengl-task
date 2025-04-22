#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>

// 函数声明
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int compileShader(unsigned int type, const char* source);
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);

// 窗口设置
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// 顶点着色器源码 (GLSL)
// 处理顶点位置变换
const char *vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos; // 顶点位置输入

    uniform mat4 model;      // 模型矩阵 (物体局部坐标 -> 世界坐标)
    uniform mat4 view;       // 视图矩阵 (世界坐标 -> 观察空间)
    uniform mat4 projection; // 投影矩阵 (观察空间 -> 裁剪空间)
    uniform float pointSize; // 点大小

    void main()
    {
        // 将顶点位置通过 MVP 矩阵变换到裁剪空间
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        gl_PointSize = pointSize; // 设置点大小
    }
)";

// 片段着色器源码 (GLSL)
// 设置片段颜色
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

// 全局变量，用于创建球体
std::vector<float> createSphere(float radius, int sectorCount, int stackCount) {
    std::vector<float> vertices;
    
    float x, y, z, xy;                  // 顶点坐标
    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    // 生成球体顶点
    for(int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);

        for(int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;
            
            // 顶点位置
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    return vertices;
}

void drawOrbit(unsigned int shaderProgram, float radius, glm::mat4& view, glm::mat4& projection, 
               float tiltAngle = 0.0f, glm::vec3 tiltAxis = glm::vec3(1.0f, 0.0f, 0.0f)) {
    // 获取uniform变量位置
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    unsigned int pointSizeLoc = glGetUniformLocation(shaderProgram, "pointSize");
    
    // 设置视图和投影矩阵
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    // 设置轨道颜色和点大小
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);  // 轨道灰色
    glUniform1f(pointSizeLoc, 1.0f);
    
    // 创建模型矩阵
    glm::mat4 model = glm::mat4(1.0f);
    if (tiltAngle != 0.0f) {
        model = glm::rotate(model, tiltAngle, tiltAxis);
    }
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // 生成轨道顶点
    const int segments = 100;
    std::vector<float> orbitVertices;
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * M_PI * float(i) / float(segments);
        float x = radius * cosf(theta);
        float z = radius * sinf(theta);
        orbitVertices.push_back(x);
        orbitVertices.push_back(0.0f);
        orbitVertices.push_back(z);
    }

    // 创建并绑定VAO/VBO
    unsigned int orbitVAO, orbitVBO;
    glGenVertexArrays(1, &orbitVAO);
    glGenBuffers(1, &orbitVBO);

    glBindVertexArray(orbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVertices.size() * sizeof(float), orbitVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 绘制
    glDrawArrays(GL_LINE_LOOP, 0, segments);

    // 清理
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &orbitVBO);
    glDeleteVertexArrays(1, &orbitVAO);
}

int main()
{
    // 1. 初始化 GLFW
    // -------------------------------------
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    // 设置 OpenGL 版本 (这里使用 3.3) 和核心模式
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Mac OS X 需要
#endif

    // 2. 创建 GLFW 窗口
    // -------------------------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "简单太阳系模拟", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    // 将窗口的 OpenGL 上下文设置为当前线程的主上下文
    glfwMakeContextCurrent(window);
    // 注册窗口大小变化的回调函数
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. 初始化 GLAD
    // -------------------------------------
    // 加载所有 OpenGL 函数指针
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate(); // 如果 GLAD 初始化失败，也需要终止 GLFW
        return -1;
    }

    // 4. 构建和编译着色器程序
    // -------------------------------------
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // 5. 设置顶点数据和缓冲区
    // -------------------------------------
    // 生成球体顶点数据
    std::vector<float> sphereVertices = createSphere(1.0f, 36, 18);
    
    // 创建顶点缓冲对象 (VBO) 和顶点数组对象 (VAO)
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO); // 生成 VAO
    glGenBuffers(1, &VBO);      // 生成 VBO

    glBindVertexArray(VAO); // 绑定 VAO

    glBindBuffer(GL_ARRAY_BUFFER, VBO); // 绑定 VBO 到 GL_ARRAY_BUFFER 目标
    // 将顶点数据复制到 VBO
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);

    // 设置顶点属性指针
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); // 启用顶点属性 location 0

    // 解绑 VBO 和 VAO (非必须，但好习惯)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 启用深度测试，这样物体的前后关系才能正确显示
    glEnable(GL_DEPTH_TEST);
    // 允许在着色器中设置点的大小
    glEnable(GL_PROGRAM_POINT_SIZE);
    // 启用反锯齿
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // 6. 渲染循环 (Render Loop)
    // -------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        // a. 处理输入
        processInput(window);

        // b. 渲染指令
        // 清除颜色缓冲和深度缓冲
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // 更暗的背景，像深太空
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 激活着色器程序
        glUseProgram(shaderProgram);

        // 获取 uniform 变量的位置
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
        unsigned int pointSizeLoc = glGetUniformLocation(shaderProgram, "pointSize");

        // 创建变换矩阵
        // 投影矩阵 (透视投影)
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // 视图矩阵 (摄像机)
        // 让摄像机稍微向后并向上移动一点，看向原点
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 15.0f, 30.0f), // 摄像机位置
                                     glm::vec3(0.0f, 0.0f, 0.0f), // 目标位置 (原点)
                                     glm::vec3(0.0f, 1.0f, 0.0f)); // 上向量
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        float timeValue = (float)glfwGetTime(); // 获取当前时间，用于动画
        
        // 绘制轨道
        // 行星1轨道
        float orbitRadius1 = 10.0f;
        glLineWidth(1.0f);
        drawOrbit(shaderProgram, orbitRadius1, view, projection);
        
        // 行星2轨道
        float orbitRadius2 = 16.0f;
        drawOrbit(shaderProgram, orbitRadius2, view, projection);
        
        // 行星3轨道（带倾斜）
        float orbitRadius3 = 22.0f;
        drawOrbit(shaderProgram, orbitRadius3, view, projection, glm::radians(20.0f));
        
        // 绑定 VAO (因为我们只有一个物体类型，所以在循环外绑定一次也可以)
        glBindVertexArray(VAO);

        // --- 绘制太阳 ---
        glm::mat4 model = glm::mat4(1.0f); // 初始化模型矩阵为单位矩阵
        model = glm::scale(model, glm::vec3(3.0f)); // 放大太阳
        // 添加太阳自转
        model = glm::rotate(model, timeValue * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(colorLoc, 1.0f, 0.7f, 0.0f); // 设置太阳颜色为橙黄色
        glUniform1f(pointSizeLoc, 40.0f); // 设置太阳的点大小
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3); // 绘制球体

        // --- 绘制行星 ---
        
        // 行星 1 (例如：地球)
        float speed1 = 0.5f;
        float angle1 = timeValue * speed1;
        float selfRotationSpeed1 = 1.0f; // 地球自转速度
        
        glm::mat4 planetModel1 = glm::mat4(1.0f);
        // 1. 旋转（轨道运动）
        planetModel1 = glm::rotate(planetModel1, angle1, glm::vec3(0.0f, 1.0f, 0.0f)); // 绕 Y 轴旋转
        // 2. 平移到轨道半径处
        planetModel1 = glm::translate(planetModel1, glm::vec3(orbitRadius1, 0.0f, 0.0f));
        // 保存行星位置用于月球
        glm::vec3 planetPosition = glm::vec3(
            planetModel1[3][0], 
            planetModel1[3][1], 
            planetModel1[3][2]
        );
        // 3. 添加自转
        planetModel1 = glm::rotate(planetModel1, timeValue * selfRotationSpeed1, glm::vec3(0.0f, 1.0f, 0.0f));
        // 4. 倾斜地球自转轴
        planetModel1 = glm::rotate(planetModel1, glm::radians(23.5f), glm::vec3(0.0f, 0.0f, 1.0f));
        // 5. 缩放行星大小
        planetModel1 = glm::scale(planetModel1, glm::vec3(1.0f));
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planetModel1));
        glUniform3f(colorLoc, 0.0f, 0.5f, 1.0f); // 设置行星1颜色为蓝色
        glUniform1f(pointSizeLoc, 20.0f);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // 绘制月球
        float moonOrbitRadius = 2.0f;
        float moonSpeed = 3.0f;
        float moonAngle = timeValue * moonSpeed;
        
        glm::mat4 moonModel = glm::mat4(1.0f);
        // 使用行星位置作为起点
        moonModel[3][0] = planetPosition.x;
        moonModel[3][1] = planetPosition.y;
        moonModel[3][2] = planetPosition.z;
        // 月球轨道旋转
        moonModel = glm::rotate(moonModel, moonAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        // 平移到月球轨道半径
        moonModel = glm::translate(moonModel, glm::vec3(moonOrbitRadius, 0.0f, 0.0f));
        // 缩放月球大小
        moonModel = glm::scale(moonModel, glm::vec3(0.3f));
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(moonModel));
        glUniform3f(colorLoc, 0.8f, 0.8f, 0.8f); // 设置月球颜色为浅灰色
        glUniform1f(pointSizeLoc, 8.0f);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // 行星 2 (例如：火星)
        float speed2 = 0.35f;
        float angle2 = timeValue * speed2;
        float selfRotationSpeed2 = 0.8f;
        
        glm::mat4 planetModel2 = glm::mat4(1.0f);
        planetModel2 = glm::rotate(planetModel2, angle2, glm::vec3(0.0f, 1.0f, 0.0f));
        planetModel2 = glm::translate(planetModel2, glm::vec3(orbitRadius2, 0.0f, 0.0f));
        // 添加自转
        planetModel2 = glm::rotate(planetModel2, timeValue * selfRotationSpeed2, glm::vec3(0.0f, 1.0f, 0.0f));
        planetModel2 = glm::scale(planetModel2, glm::vec3(0.8f));
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planetModel2));
        glUniform3f(colorLoc, 1.0f, 0.4f, 0.2f); // 设置行星2颜色为橙红色
        glUniform1f(pointSizeLoc, 18.0f);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // 行星 3 (例如：带倾斜轨道的行星)
        float speed3 = 0.25f;
        float angle3 = timeValue * speed3;
        float selfRotationSpeed3 = 0.6f;
        
        glm::mat4 planetModel3 = glm::mat4(1.0f);
        // 先倾斜轨道平面
        planetModel3 = glm::rotate(planetModel3, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        // 再进行轨道旋转
        planetModel3 = glm::rotate(planetModel3, angle3, glm::vec3(0.0f, 1.0f, 0.0f));
        // 平移
        planetModel3 = glm::translate(planetModel3, glm::vec3(orbitRadius3, 0.0f, 0.0f));
        // 添加自转
        planetModel3 = glm::rotate(planetModel3, timeValue * selfRotationSpeed3, glm::vec3(0.2f, 1.0f, 0.0f));
        // 缩放
        planetModel3 = glm::scale(planetModel3, glm::vec3(1.2f));
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planetModel3));
        glUniform3f(colorLoc, 0.7f, 0.2f, 0.9f); // 设置行星3颜色为紫色
        glUniform1f(pointSizeLoc, 24.0f);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertices.size() / 3);

        // c. 交换缓冲区和检查事件
        glfwSwapBuffers(window); // 交换前后缓冲区，显示渲染结果
        glfwPollEvents();      // 处理如图形界面事件、键盘鼠标事件等
    }

    // 7. 清理资源
    // -------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate(); // 终止 GLFW，释放资源
    return 0;
}

// 处理输入: 查询 GLFW 是否按下了相关按键，并在本帧做出反应
void processInput(GLFWwindow *window)
{
    // 如果按下 ESC 键，设置窗口的 "ShouldClose" 属性为 true，从而结束循环
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// 当窗口大小改变时，这个回调函数会被调用
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // 确保视口匹配新的窗口尺寸
    // 注意：对于高 DPI 屏幕，width 和 height 可能远大于原始的 SCR_WIDTH 和 SCR_HEIGHT
    glViewport(0, 0, width, height);
}

// 编译单个着色器
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type); // 创建着色器对象
    glShaderSource(id, 1, &source, nullptr); // 附加源码
    glCompileShader(id); // 编译

    // 检查编译错误
    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "错误::着色器::" << (type == GL_VERTEX_SHADER ? "顶点" : "片段") << "::编译失败\n" << infoLog << std::endl;
        glDeleteShader(id); // 删除失败的着色器
        return 0;
    }
    return id;
}

// 创建链接后的着色器程序
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // 编译顶点和片段着色器
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        return 0; // 如果任一着色器编译失败，则返回 0
    }

    // 创建着色器程序对象
    unsigned int program = glCreateProgram();
    // 附加着色器
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    // 链接着色器程序
    glLinkProgram(program);

    // 检查链接错误
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "错误::着色器::程序::链接失败\n" << infoLog << std::endl;
        glDeleteProgram(program); // 删除失败的程序
        program = 0;
    }

    // 删除不再需要的着色器对象（它们已经链接到程序中了）
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
