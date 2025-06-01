#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


const char *vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoords; 

    out vec2 TexCoords;

    void main()
    {
        TexCoords = aTexCoords;
        gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    }
)";

const char *fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords; 

    uniform vec2 iResolution;

    
    uniform vec3 cameraPos;
    uniform vec3 cameraTarget;
    uniform vec3 cameraUp;
    uniform float cameraFov; 

    
    uniform vec3 sphereCenter;
    uniform float sphereRadius;
    uniform vec4 sphereColorAlpha; 

    
    uniform vec3 cubeMin;
    uniform vec3 cubeMax;
    uniform vec4 cubeColorAlpha; 

    
    uniform vec3 planeNormal;
    uniform float planeD; 
    uniform vec3 checkerColor1;
    uniform vec3 checkerColor2;
    uniform float checkerScale;

    const int MAX_BOUNCES = 3; 
    const float EPSILON = 0.001; 

    
    
    float intersectSphere(vec3 ro, vec3 rd, vec3 sc, float sr) {
        vec3 oc = ro - sc;
        float a = dot(rd, rd); 
        float b = 2.0 * dot(oc, rd);
        float c = dot(oc, oc) - sr*sr;
        float discriminant = b*b - 4.0*a*c;
        if (discriminant < 0.0) {
            return -1.0;
        } else {
            float t1 = (-b - sqrt(discriminant)) / (2.0*a);
            float t2 = (-b + sqrt(discriminant)) / (2.0*a);
            if (t1 > EPSILON && (t1 < t2 || t2 < EPSILON)) return t1;
            if (t2 > EPSILON) return t2;
            return -1.0;
        }
    }

    
    
    float intersectAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out vec3 outHitNormal) {
        vec3 invDir = 1.0 / rd;
        vec3 tMinPlanes = (bmin - ro) * invDir;
        vec3 tMaxPlanes = (bmax - ro) * invDir;

        vec3 t1 = min(tMinPlanes, tMaxPlanes);
        vec3 t2 = max(tMinPlanes, tMaxPlanes);

        float tNear = max(max(t1.x, t1.y), t1.z);
        float tFar = min(min(t2.x, t2.y), t2.z);

        if (tNear < tFar && tFar > EPSILON) {
            if (tNear > EPSILON) { 
                vec3 hitPoint = ro + rd * tNear;
                vec3 box_center = (bmin + bmax) * 0.5;
                vec3 local_hit_point = hitPoint - box_center;
                vec3 box_half_extents = (bmax - bmin) * 0.5;
                
                
                vec3 abs_local_hp = abs(local_hit_point);
                if (abs_local_hp.x > abs_local_hp.y && abs_local_hp.x > abs_local_hp.z) {
                    outHitNormal = vec3(sign(local_hit_point.x), 0.0, 0.0);
                } else if (abs_local_hp.y > abs_local_hp.z) {
                    outHitNormal = vec3(0.0, sign(local_hit_point.y), 0.0);
                } else {
                    outHitNormal = vec3(0.0, 0.0, sign(local_hit_point.z));
                }
                return tNear;
            }
            
            
        }
        return -1.0;
    }

    
    
    float intersectPlane(vec3 ro, vec3 rd, vec3 pn, float pd) {
        float denom = dot(rd, pn);
        if (abs(denom) > EPSILON) { 
            float t = (pd - dot(ro, pn)) / denom;
            if (t > EPSILON) return t;
        }
        return -1.0;
    }

    void main()
    {
        
        
        vec2 uv_centered = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y; 

        
        vec3 camForward = normalize(cameraTarget - cameraPos);
        vec3 camRight = normalize(cross(camForward, cameraUp));
        vec3 camActualUp = normalize(cross(camRight, camForward)); 
        
        float focalLength = 1.0 / tan(radians(cameraFov) * 0.5);
        vec3 rayDir = normalize(uv_centered.x * camRight + uv_centered.y * camActualUp + focalLength * camForward);
        vec3 rayOrigin = cameraPos;

        
        vec3 finalColor = vec3(0.0);
        float transmission = 1.0; 

        vec3 currentRayOrigin = rayOrigin;
        vec3 currentRayDir = rayDir;

        for (int i = 0; i < MAX_BOUNCES; ++i) {
            if (transmission < 0.01) break; 

            float t_hit = 1e20; 
            vec4 hitObjectColorAlpha = vec4(0.0); 
            vec3 hitNormal = vec3(0.0);
            int hitType = 0; 

            
            float t_sphere = intersectSphere(currentRayOrigin, currentRayDir, sphereCenter, sphereRadius);
            if (t_sphere > EPSILON && t_sphere < t_hit) {
                t_hit = t_sphere;
                hitObjectColorAlpha = sphereColorAlpha;
                hitNormal = normalize((currentRayOrigin + currentRayDir * t_sphere) - sphereCenter);
                hitType = 1;
            }

            
            vec3 cubeHitNormal;
            float t_cube = intersectAABB(currentRayOrigin, currentRayDir, cubeMin, cubeMax, cubeHitNormal);
            if (t_cube > EPSILON && t_cube < t_hit) {
                t_hit = t_cube;
                hitObjectColorAlpha = cubeColorAlpha;
                hitNormal = cubeHitNormal;
                hitType = 2;
            }

            if (hitType > 0) { 
                
                
                
                
                vec3 litColor = hitObjectColorAlpha.rgb; 

                finalColor += transmission * litColor * hitObjectColorAlpha.a;
                transmission *= (1.0 - hitObjectColorAlpha.a);
                currentRayOrigin = currentRayOrigin + currentRayDir * (t_hit + EPSILON * 2.0); 
            } else { 
                float t_plane = intersectPlane(currentRayOrigin, currentRayDir, planeNormal, planeD);
                if (t_plane > EPSILON) {
                    vec3 planeHitPoint = currentRayOrigin + currentRayDir * t_plane;
                    vec2 boardCoords;
                    
                    if (abs(planeNormal.z) > 0.99) { 
                        boardCoords = planeHitPoint.xy;
                    } else if (abs(planeNormal.y) > 0.99) { 
                        boardCoords = planeHitPoint.xz;
                    } else { 
                        boardCoords = planeHitPoint.yz;
                    }

                    float pattern = mod(floor(boardCoords.x * checkerScale) + floor(boardCoords.y * checkerScale), 2.0);
                    vec3 checkerCol = (pattern < 0.5) ? checkerColor1 : checkerColor2;
                    finalColor += transmission * checkerCol;
                } else {
                    
                    finalColor += transmission * vec3(0.1, 0.1, 0.15); 
                }
                break; 
            }
        }
        FragColor = vec4(finalColor, 1.0); 
    }
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);


unsigned int compileShader(GLenum type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}


unsigned int createShaderProgram(unsigned int vertexShader, unsigned int fragmentShader) {
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    return program;
}


int main() {
    
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Task 3: Ray Tracing", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shaderProgram = createShaderProgram(vertexShader, fragmentShader);
    glDeleteShader(vertexShader); 
    glDeleteShader(fragmentShader);

    
    float quadVertices[] = { 
        
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0); 

    
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        
        glUniform2f(glGetUniformLocation(shaderProgram, "iResolution"), (float)SCR_WIDTH, (float)SCR_HEIGHT);
        
        
        glm::vec3 camPos = glm::vec3(0.0f, 0.5f, 4.0f); 
        glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, glm::value_ptr(camPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "cameraTarget"), 1, glm::value_ptr(camTarget));
        glUniform3fv(glGetUniformLocation(shaderProgram, "cameraUp"), 1, glm::value_ptr(camUp));
        glUniform1f(glGetUniformLocation(shaderProgram, "cameraFov"), 60.0f); 

        
        glm::vec3 sCenter(-0.8f, 0.0f, 0.0f); 
        float sRadius = 0.7f;
        glm::vec4 sColorAlpha(1.0f, 0.3f, 0.3f, 0.5f); 
        glUniform3fv(glGetUniformLocation(shaderProgram, "sphereCenter"), 1, glm::value_ptr(sCenter));
        glUniform1f(glGetUniformLocation(shaderProgram, "sphereRadius"), sRadius);
        glUniform4fv(glGetUniformLocation(shaderProgram, "sphereColorAlpha"), 1, glm::value_ptr(sColorAlpha));

        
        glm::vec3 cMin(0.4f, -0.6f, -0.4f); 
        glm::vec3 cMax(1.4f, 0.6f, 0.6f);   
        glm::vec4 cColorAlpha(0.3f, 0.3f, 1.0f, 0.65f); 
        glUniform3fv(glGetUniformLocation(shaderProgram, "cubeMin"), 1, glm::value_ptr(cMin));
        glUniform3fv(glGetUniformLocation(shaderProgram, "cubeMax"), 1, glm::value_ptr(cMax));
        glUniform4fv(glGetUniformLocation(shaderProgram, "cubeColorAlpha"), 1, glm::value_ptr(cColorAlpha));
        
        
        glm::vec3 pNormal(0.0f, 0.0f, 1.0f); 
        float pD = -2.0f; 
        glm::vec3 chkCol1(0.8f, 0.8f, 0.8f); 
        glm::vec3 chkCol2(0.3f, 0.3f, 0.3f); 
        float chkScale = 1.5f;
        glUniform3fv(glGetUniformLocation(shaderProgram, "planeNormal"), 1, glm::value_ptr(pNormal));
        glUniform1f(glGetUniformLocation(shaderProgram, "planeD"), pD);
        glUniform3fv(glGetUniformLocation(shaderProgram, "checkerColor1"), 1, glm::value_ptr(chkCol1));
        glUniform3fv(glGetUniformLocation(shaderProgram, "checkerColor2"), 1, glm::value_ptr(chkCol2));
        glUniform1f(glGetUniformLocation(shaderProgram, "checkerScale"), chkScale);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}