#include "utility.h"

// Global variables
// ----------------
float deltaTime;

int main()
{
    // Window Initialization
    GLFWwindow* window = InitWindow();
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // UI Initialization
    ImGuiIO& io = InitUI(window);

    // Cameras
    glm::vec3 cameraPosition(10.0f, 10.0f, 50.0f);
    Camera cam1(cameraPosition);

    // Models
    Model scene("models/floor/scene.obj");
    Model light("models/sun/sun.obj");

    // Shaders
    Shader modelShader("shaders/model.vert", "shaders/model.frag");
    Shader simpleDepthShader("shaders/depthShader.vert", "shaders/depthShader.frag");
    Shader lightShader("shaders/light.vert", "shaders/light.frag");

    // Transformation matrices
    glm::mat4 model(1.0f);
    glm::mat4 view;
    glm::mat4 projection;


    // texture & framebuffer for storing depth values
    unsigned int depthMap;
    unsigned int depthMapFBO = createDepthMapFB(depthMap);

    // Game loop
    while(!glfwWindowShouldClose(window))
    {
        glClearColor(window_color.x, window_color.y, window_color.z, 1.0f);
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
        view = cam1.GetViewMatrix();
        projection = glm::perspective(glm::radians(45.0f), (float)window_width / window_height, 0.1f, 1000.0f);



        // Create a light object
        Light light1;
        light1.color = lightColor;
        light1.position = lightPos;


        // 1. first render to depth map
        float near_plane = -10.0f, far_plane = 100.0f;
        glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(light1.position,
                                  glm::vec3( 0.0f, 0.0f,  0.0f),
                                  glm::vec3( 0.0f, 1.0f,  0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        simpleDepthShader.use();
        simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        simpleDepthShader.setMat4("model", model);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        // Drawing to the depth map
        glCullFace(GL_FRONT);
        scene.Draw(simpleDepthShader);
        glCullFace(GL_BACK); // reset original culling face


        // go back to default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // then render scene as normal with shadow mapping ( using da depth map )
        glViewport(0, 0, window_width, window_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        modelShader.use();
        modelShader.setMVP(model, view, projection);
        modelShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        modelShader.setVec3("viewPos", cam1.Position);
        modelShader.setVec3("lightPos", light1.position);
        modelShader.setVec3("lightColor", light1.color);

        // planetShader.setInt("shadowMap", 1);
        modelShader.setInt("shadowMap", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        scene.Draw(modelShader);

        // Draw a light to show where the light is
        lightShader.use();
        glm::mat4 lightModel(1.0f);
        lightModel = glm::translate(lightModel, light1.position);
        lightShader.setMVP(lightModel, view, projection);
        lightShader.setVec3("color", light1.color);
        light.Draw(lightShader);

        // UI Stuff
        // start ImGUI frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Window stuff
        ImGui::Begin("Window");
        ImGui::Text("framerate: %.2f fps", io.Framerate);
        ImGui::ColorEdit3("background color", &window_color.x);
        ImGui::End();

        // Depth map stuff
        ImGui::Begin("DepthMap");
        {
            ImGui::BeginChild("GameRender");
            ImVec2 wsize = ImGui::GetWindowSize();
            ImGui::Image((ImTextureID)depthMap, wsize, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::EndChild();
        }
        ImGui::End();

        // Light stuff
        ImGui::Begin("light");
        ImGui::SliderFloat3("position", &lightPos.x, -50.0f, 50.0f);
        ImGui::ColorEdit3("color", &lightColor.x);
        ImGui::End();

        // ImGUI rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}
