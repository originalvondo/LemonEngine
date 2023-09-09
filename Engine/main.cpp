#include "utility.h"

// Global variables
// ----------------
float deltaTime;

float lightPosArr[] = { -2.0f, 4.0f, -1.0f };

int main()
{
    // Window Initialization
    GLFWwindow* window = InitWindow();
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);

    // UI Initialization
    ImGuiIO& io = InitUI(window);

    // Cameras
    glm::vec3 cameraPosition(0.0f, 0.0f, 10.0f);
    Camera cam1(cameraPosition);

    // Models
    Model planet("models/floor/floor.obj");
    Model light("models/sun/sun.obj");

    // Shaders
    Shader planetShader("shaders/model.vert", "shaders/model.frag");
    Shader simpleDepthShader("shaders/depthShader.vert", "shaders/depthShader.frag");
    Shader lightShader("shaders/light.vert", "shaders/light.frag");

    // Transformation matrices
    glm::mat4 view;
    glm::mat4 projection;


    // Framebuffers
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    float lastFrame = 0.0f;
    while(!glfwWindowShouldClose(window))
    {
        glClearColor(window_color[0], window_color[1], window_color[2], 1.0f);
        glm::vec3 lightPos(lightPosArr[0], lightPosArr[1], lightPosArr[2]);

        // polling events
        glfwPollEvents();

        // Setup deltaTime
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process Key inputs
        processInputs(window, cam1, deltaTime);
        // Camera Inputs
        if(!io.WantCaptureMouse)
            cam1.ProcessMouseMovement(window);

        // Setup Transformation Matrices
        glm::mat4 model(1.0f);
        view = cam1.GetViewMatrix();
        projection = glm::perspective(glm::radians(45.0f), (float)window_width / window_height, 0.1f, 1000.0f);

        // 1. first render to depth map
        float near_plane = 1.0f, far_plane = 100.0f;
        glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(lightPos,
                                  glm::vec3( 0.0f, 0.0f,  0.0f),
                                  glm::vec3( 0.0f, 1.0f,  0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        simpleDepthShader.use();
        simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        simpleDepthShader.setMat4("model", model);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        // Drawing to the depth map (peter panning solved)
        glCullFace(GL_FRONT);
        planet.Draw(simpleDepthShader);
        glCullFace(GL_BACK); // reset original culling face


        // Now, bind to the default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // 2. then render scene as normal with shadow mapping (using depth map)
        glm::vec3 lightColor(lightColorArr[0], lightColorArr[1], lightColorArr[2]);
        glViewport(0, 0, window_width, window_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        planetShader.use();
        glm::mat4 PlanetModel(1.0f);
        planetShader.setMat4("model", PlanetModel);
        planetShader.setMat4("view", view);
        planetShader.setMat4("projection", projection);
        planetShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        planetShader.setVec3("viewPos", cam1.Position);
        planetShader.setVec3("lightPos", lightPos);
        planetShader.setVec3("lightColor", lightColor);

        // planetShader.setInt("shadowMap", 1);
        planetShader.setInt("shadowMap", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        planet.Draw(planetShader);


        // Draw a light to show where the light is
        lightShader.use();
        glm::mat4 lightModel(1.0f);
        lightModel = glm::translate(lightModel, lightPos);
        lightShader.setMat4("model", lightModel);
        lightShader.setMat4("view", view);
        lightShader.setMat4("projection", projection);
        lightShader.setVec3("color", lightColor);
        light.Draw(lightShader);

        // UI Stuff
        // start ImGUI frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Window stuff
        ImGui::Begin("Window");
        ImGui::Text("framerate: %.2f fps", io.Framerate);
        ImGui::ColorEdit3("background color", window_color);
        ImGui::End();

        // Depth map stuff
        ImGui::Begin("DepthMap");
        {
            // Using a Child allow to fill all the space of the window.
            // It also alows customization
            ImGui::BeginChild("GameRender");
            // Get the size of the child (i.e. the whole draw size of the windows).
            ImVec2 wsize = ImGui::GetWindowSize();
            // Because I use the texture from OpenGL, I need to invert the V from the UV.
            ImGui::Image((ImTextureID)depthMap, wsize, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::EndChild();
        }
        ImGui::End();

        // Light stuff
        ImGui::Begin("light");
        ImGui::SliderFloat3("position", lightPosArr, -20.0f, 20.0f);
        ImGui::ColorEdit3("color", lightColorArr);
        ImGui::End();

        // ImGUI rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}
