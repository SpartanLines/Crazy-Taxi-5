#include <application.hpp>
#include "directional_light_scene.hpp"

#include <mesh/mesh_utils.hpp>
#include <textures/texture_utils.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void DirectionalLightScene::Initialize() {
    shader = new Shader();
    shader->attach("assets/shaders/directional.vert", GL_VERTEX_SHADER);
    shader->attach("assets/shaders/directional.frag", GL_FRAGMENT_SHADER);
    shader->link();

    skyShader = new Shader();
    skyShader->attach("assets/shaders/sky.vert", GL_VERTEX_SHADER);
    skyShader->attach("assets/shaders/sky.frag", GL_FRAGMENT_SHADER);
    skyShader->link();
    
    ground = MeshUtils::Plane({0,0}, {5,5});
    sky = MeshUtils::Box();
    model = MeshUtils::LoadObj("assets/models/suzanne.obj");

    metal[ALBEDO] = TextureUtils::Load2DTextureFromFile("assets/textures/Metal_col.jpg");
    metal[SPECULAR] = TextureUtils::Load2DTextureFromFile("assets/textures/Metal_spc.jpg");
    metal[ROUGHNESS] = TextureUtils::Load2DTextureFromFile("assets/textures/Metal_rgh.jpg");
    metal[AMBIENT_OCCLUSION] = TextureUtils::Load2DTextureFromFile("assets/textures/Suzanne_ao.jpg");
    metal[EMISSIVE] = TextureUtils::SingleColor({0,0,0,1});

    wood[ALBEDO] = TextureUtils::Load2DTextureFromFile("assets/textures/Wood_col.jpg");
    wood[SPECULAR] = TextureUtils::Load2DTextureFromFile("assets/textures/Wood_spc.jpg");
    wood[ROUGHNESS] = TextureUtils::Load2DTextureFromFile("assets/textures/Wood_rgh.jpg");
    wood[AMBIENT_OCCLUSION] = TextureUtils::Load2DTextureFromFile("assets/textures/Suzanne_ao.jpg");
    wood[EMISSIVE] = TextureUtils::SingleColor({0,0,0,1});

    asphalt[ALBEDO] = TextureUtils::Load2DTextureFromFile("assets/textures/Asphalt_col.jpg");
    asphalt[SPECULAR] = TextureUtils::Load2DTextureFromFile("assets/textures/Asphalt_spc.jpg");
    asphalt[ROUGHNESS] = TextureUtils::Load2DTextureFromFile("assets/textures/Asphalt_rgh.jpg");
    asphalt[AMBIENT_OCCLUSION] = TextureUtils::Load2DTextureFromFile("assets/textures/Suzanne_ao.jpg");
    asphalt[EMISSIVE] = TextureUtils::Load2DTextureFromFile("assets/textures/Asphalt_em.jpg");

    checkers[ALBEDO] = TextureUtils::CheckerBoard({2048, 2048}, {128, 128}, {1,1,1,1}, {0,0,0,1});
    checkers[SPECULAR] = TextureUtils::CheckerBoard({2048, 2048}, {128, 128}, {0.2f,0.2f,0.2f,1}, {1,1,1,1});
    checkers[ROUGHNESS] = TextureUtils::CheckerBoard({2048, 2048}, {128, 128}, {0.9f,0.9f,0.9f,1}, {0.4f,0.4f,0.4f,1});
    checkers[AMBIENT_OCCLUSION] = TextureUtils::SingleColor({1,1,1,1});
    checkers[EMISSIVE] = TextureUtils::SingleColor({0,0,0,1});

    camera = new Camera();
    glm::ivec2 windowSize = getApplication()->getWindowSize();
    camera->setupPerspective(glm::pi<float>()/2, (float)windowSize.x / windowSize.y, 0.1f, 1000.0f);
    camera->setUp({0, 1, 0});

    controller = new FlyCameraController(this, camera);
    controller->setYaw(-glm::half_pi<float>());
    controller->setPitch(-glm::quarter_pi<float>());
    controller->setPosition({0, 2, 5});

    sunYaw = sunPitch = glm::quarter_pi<float>();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.88f,0.68f,0.15f,0.0f);
}

void DirectionalLightScene::Update(double delta_time) {
    controller->update(delta_time);
    Keyboard* kb = getKeyboard();

    float pitch_speed = 1.0f, yaw_speed = 1.0f;

    if(kb->isPressed(GLFW_KEY_I)) sunPitch += (float)delta_time * pitch_speed;
    if(kb->isPressed(GLFW_KEY_K)) sunPitch -= (float)delta_time * pitch_speed;
    if(kb->isPressed(GLFW_KEY_L)) sunYaw += (float)delta_time * yaw_speed;
    if(kb->isPressed(GLFW_KEY_J)) sunYaw -= (float)delta_time * yaw_speed;
    
    if(sunPitch < -glm::half_pi<float>()) sunPitch = -glm::half_pi<float>();
    if(sunPitch > glm::half_pi<float>()) sunPitch = glm::half_pi<float>();
    sunYaw = glm::wrapAngle(sunYaw);
}

inline glm::vec3 getTimeOfDayMix(float sunPitch){
    sunPitch /= glm::half_pi<float>();
    if(sunPitch > 0){
        float noon = glm::smoothstep(0.0f, 0.5f, sunPitch);
        return {noon, 1.0f - noon, 0};
    } else {
        float dusk = glm::smoothstep(0.0f, 0.25f, -sunPitch);
        return {0, 1.0f - dusk, dusk};
    }
}

void DirectionalLightScene::Draw() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear colors and depth

    glm::mat4 VP = camera->getVPMatrix();
    glm::vec3 cam_pos = camera->getPosition();
    glm::vec3 sun_direction = glm::vec3(glm::cos(sunYaw), 0, glm::sin(sunYaw)) * glm::cos(sunPitch) + glm::vec3(0, glm::sin(sunPitch), 0);

    const glm::vec3 noonSkyColor = {0.53f, 0.81f, 0.98f};
    const glm::vec3 sunsetSkyColor = {0.99f, 0.37f, 0.33f};
    const glm::vec3 duskSkyColor = {0.04f, 0.05f, 0.19f};

    const glm::vec3 noonSunColor = {0.9f, 0.8f, 0.6f};
    const glm::vec3 sunsetSunColor = {0.8f, 0.6f, 0.4f};
    const glm::vec3 duskSunColor = {0.0f, 0.0f, 0.0f};
    
    glm::vec3 mix = getTimeOfDayMix(sunPitch);

    glm::vec3 skyColor = mix.x * noonSkyColor + mix.y * sunsetSkyColor + mix.z * duskSkyColor;
    glm::vec3 sunColor = mix.x * noonSunColor + mix.y * sunsetSunColor + mix.z * duskSunColor;

    shader->use();
    shader->set("VP", VP);
    shader->set("cam_pos", cam_pos);
    shader->set("light.color", sunColor);
    shader->set("light.direction", -sun_direction);
    shader->set("ambient", 0.5f*skyColor);

    shader->set("material.albedo", 0);
    shader->set("material.specular", 1);
    shader->set("material.roughness", 2);
    shader->set("material.ambient_occlusion", 3);
    shader->set("material.emissive", 4);

    shader->set("material.albedo_tint", {1,1,1});
    shader->set("material.specular_tint", {1,1,1});
    shader->set("material.roughness_scale", 1.0f);
    shader->set("material.emissive_tint", {1,1,1});

    glm::mat4 ground_mat = glm::scale(glm::mat4(), glm::vec3(50, 1, 50));
    shader->set("M", ground_mat);
    shader->set("M_it", glm::transpose(glm::inverse(ground_mat)));
    for(int i = 0; i < 5; i++){
        glActiveTexture(GL_TEXTURE0+i);
        checkers[i]->bind();
    }
    ground->draw();

    glm::mat4 model1_mat = glm::translate(glm::mat4(), {-4, 1, 0});
    shader->set("M", model1_mat);
    shader->set("M_it", glm::transpose(glm::inverse(model1_mat)));
    for(int i = 0; i < 5; i++){
        glActiveTexture(GL_TEXTURE0+i);
        metal[i]->bind();
    }
    model->draw();

    glm::mat4 model2_mat = glm::translate(glm::mat4(), {0, 1, 0});
    shader->set("M", model2_mat);
    shader->set("M_it", glm::transpose(glm::inverse(model2_mat)));
    for(int i = 0; i < 5; i++){
        glActiveTexture(GL_TEXTURE0+i);
        wood[i]->bind();
    }
    model->draw();

    glm::mat4 model3_mat = glm::translate(glm::mat4(), {4, 1, 0});
    shader->set("M", model3_mat);
    shader->set("M_it", glm::transpose(glm::inverse(model3_mat)));
    for(int i = 0; i < 5; i++){
        glActiveTexture(GL_TEXTURE0+i);
        asphalt[i]->bind();
    }
    float emissive_power = glm::sin((float)glfwGetTime()) + 1;
    shader->set("material.emissive_tint", glm::vec3(1,1,1) * emissive_power);
    model->draw();

    //Draw SkyBox
    skyShader->use();
    skyShader->set("VP", VP);
    skyShader->set("cam_pos", cam_pos);
    skyShader->set("M", glm::translate(glm::mat4(), cam_pos));
    skyShader->set("sun_direction", sun_direction);
    skyShader->set("sun_size", 0.02f);
    skyShader->set("sun_halo_size", 0.02f);
    skyShader->set("sun_brightness", 1.0f);
    skyShader->set("sun_color", sunColor);
    skyShader->set("sky_top_color", skyColor);
    skyShader->set("sky_bottom_color", 1.0f-0.25f*(1.0f-skyColor));
    skyShader->set("sky_smoothness", 0.5f);
    glCullFace(GL_FRONT);
    sky->draw();
    glCullFace(GL_BACK);
}

void DirectionalLightScene::Finalize() {
    delete controller;
    delete camera;
    delete model;
    delete sky;
    delete ground;
    for(int i = 0; i < 5; i++){
        delete metal[i];
        delete wood[i];
        delete asphalt[i];
        delete checkers[i];
    }
    delete skyShader;
    delete shader;
}