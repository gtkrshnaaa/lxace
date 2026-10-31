#include "test_runner.hpp"
#include "cli/view_base.hpp"
#include "cli/view_home.hpp"
#include "cli/view_settings.hpp"
#include "cli/view_prompt.hpp"
#include "cli/terminal.hpp"
#include "core/engine.hpp"
#include <sstream>
#include <iostream>

using namespace lxace::cli;

TEST_CASE(ui_notification_lifecycle) {
    UiNotification notif;
    ASSERT_TRUE(notif.empty());
    ASSERT_EQ(notif.format(), "");

    notif.type = UiNotification::Type::Info;
    notif.text = "System ready";
    ASSERT_FALSE(notif.empty());
    ASSERT_EQ(notif.format(), "[INFO] System ready");

    notif.type = UiNotification::Type::Success;
    notif.text = "Saved successfully";
    ASSERT_EQ(notif.format(), "[OK] Saved successfully");

    notif.type = UiNotification::Type::Warning;
    notif.text = "Workers stopped";
    ASSERT_EQ(notif.format(), "[WARN] Workers stopped");

    notif.type = UiNotification::Type::Error;
    notif.text = "Invalid parameter";
    ASSERT_EQ(notif.format(), "[ERROR] Invalid parameter");

    notif.clear();
    ASSERT_TRUE(notif.empty());
}

TEST_CASE(view_home_rendering_and_metadata) {
    std::filesystem::path temp_dir = std::filesystem::current_path();
    lxace::core::Engine engine(temp_dir, 9090);
    engine.initialize();

    ViewHome view_home;
    view_home.set_notification(UiNotification::Type::Success, "Task completed successfully");
    ASSERT_FALSE(view_home.notification().empty());
    ASSERT_EQ(view_home.notification().format(), "[OK] Task completed successfully");

    // Capture standard output to verify home screen layout and fields
    std::stringstream buffer;
    std::streambuf* prev = std::cout.rdbuf(buffer.rdbuf());
    view_home.render(engine);
    std::cout.rdbuf(prev);

    std::string out = buffer.str();
    ASSERT_TRUE(out.find("LXACE ENGINE") != std::string::npos);
    ASSERT_TRUE(out.find("PORT: 9090") != std::string::npos);
    ASSERT_TRUE(out.find("Project :") != std::string::npos);
    ASSERT_TRUE(out.find("Settings") != std::string::npos);
    ASSERT_TRUE(out.find("Write Prompt") != std::string::npos);
    ASSERT_TRUE(out.find("Stop All") != std::string::npos);
    ASSERT_TRUE(out.find("LIVE ENGINE LOG STREAM") != std::string::npos);
    ASSERT_TRUE(out.find("[OK] Task completed successfully") != std::string::npos);
}

TEST_CASE(view_settings_inline_parameters) {
    std::filesystem::path temp_dir = std::filesystem::current_path();
    lxace::core::Engine engine(temp_dir, 9090);
    engine.initialize();

    engine.config()->set_base_url("https://api.custom.ai/v1");
    engine.config()->set_api_key("sk-secret-test-key-12345");
    engine.config()->set_model("gpt-4o");
    engine.config()->set_max_tokens(8192);
    engine.config()->set_temperature(0.42);

    ViewSettings view_settings;
    view_settings.set_notification(UiNotification::Type::Info, "Model updated");

    std::stringstream buffer;
    std::streambuf* prev = std::cout.rdbuf(buffer.rdbuf());
    view_settings.render(engine);
    std::cout.rdbuf(prev);

    std::string out = buffer.str();
    ASSERT_TRUE(out.find("LXACE CONFIGURATION SETTINGS") != std::string::npos);
    ASSERT_TRUE(out.find("Base URL") != std::string::npos);
    ASSERT_TRUE(out.find("API Key") != std::string::npos);
    ASSERT_TRUE(out.find("gpt-4o") != std::string::npos);
    ASSERT_TRUE(out.find("8192") != std::string::npos);
    ASSERT_TRUE(out.find("0.42") != std::string::npos);
    ASSERT_TRUE(out.find("[INFO] Model updated") != std::string::npos);
}

TEST_CASE(view_prompt_rendering) {
    std::filesystem::path temp_dir = std::filesystem::current_path();
    lxace::core::Engine engine(temp_dir, 9090);
    engine.initialize();

    ViewPrompt view_prompt;

    std::stringstream buffer;
    std::streambuf* prev = std::cout.rdbuf(buffer.rdbuf());
    view_prompt.render(engine);
    std::cout.rdbuf(prev);

    std::string out = buffer.str();
    ASSERT_TRUE(out.find("TASK PROMPT DISPATCHER") != std::string::npos);
    ASSERT_TRUE(out.find("Press [Enter] to submit") != std::string::npos);
    ASSERT_TRUE(out.find("Prompt:") != std::string::npos);
}
