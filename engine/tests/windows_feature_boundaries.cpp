// Exercise Windows feature refusals through the exported DLL boundary.
#include "spektrafilm/spk_engine.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string params(spk_session* session) {
    char* json = nullptr;
    require(spk_get_params(session, &json) == SPK_OK && json, "get_params failed");
    std::string result(json);
    spk_string_free(json);
    return result;
}

std::vector<uint16_t> render(spk_engine* engine, spk_session* session) {
    spk_result result{};
    const spk_status status = spk_render(session, "full", &result);
    const bool valid = status == SPK_OK && result.rgba16 && result.texture &&
                       result.width == 32 && result.height == 16;
    const std::string error = spk_last_error(engine);
    std::vector<uint16_t> pixels;
    if (valid) {
        pixels.reserve(size_t(result.width) * result.height * 4);
        for (uint32_t y = 0; y < result.height; ++y) {
            const uint16_t* row = result.rgba16 + size_t(y) * result.row_stride_px * 4;
            pixels.insert(pixels.end(), row, row + size_t(result.width) * 4);
        }
    }
    spk_result_free(&result);
    require(valid, "default render failed after a refused request: " + error);
    return pixels;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: spk_windows_feature_boundaries resources_dir\n"; return 2; }
    try {
        std::unique_ptr<spk_engine, decltype(&spk_engine_destroy)> engine(
            spk_engine_create(argv[1], nullptr), spk_engine_destroy);
        require(bool(engine), std::string("engine creation failed: ") + spk_last_error(nullptr));
        const char* capabilities = spk_capabilities(engine.get());
        require(capabilities && std::string(capabilities).find("unsupported_features") != std::string::npos,
                "Windows capabilities must disclose unported effects");

        std::vector<float> pixels(32 * 16 * 3);
        for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = 0.025f + float(i % 31) * 0.025f;
        const spk_image input{pixels.data(), 32, 16, 3};
        std::unique_ptr<spk_session, decltype(&spk_session_release)> session(
            spk_open(engine.get(), &input, nullptr, nullptr), spk_session_release);
        require(bool(session), std::string("default open failed: ") + spk_last_error(engine.get()));
        render(engine.get(), session.get());
        std::string before = params(session.get());

        struct Feature { const char* key; const char* label; };
        const std::vector<Feature> features{
            {"contrast_mask_active", "contrast mask"},
#ifndef SPK_WINDOWS_DATE_BACK
            {"overscan_active", "overscan"},
            {"date_imprint_active", "date imprint"},
#endif
        };
        for (const Feature& feature : features) {
            // Include a supported change to catch partial application before
            // the unsupported field is rejected.
            const std::string delta = std::string("{\"") + feature.key +
                                      "\":true,\"print_exposure\":1.25}";
            const spk_status status = spk_set_params(session.get(), delta.c_str(), nullptr);
            const std::string error = spk_last_error(engine.get());
            require(status == SPK_ERR_USER && error.find(feature.label) != std::string::npos &&
                    error.find("Windows Vulkan backend") != std::string::npos,
                    std::string("unclear feature refusal: ") + feature.key + ": " + error);
            require(params(session.get()) == before, std::string("refused delta mutated params: ") + feature.key);
            render(engine.get(), session.get());

            spk_session* rejected = spk_open(engine.get(), &input, delta.c_str(), nullptr);
            const std::string open_error = spk_last_error(engine.get());
            if (rejected) spk_session_release(rejected);
            require(!rejected && open_error.find(feature.label) != std::string::npos,
                    std::string("open accepted an unported effect: ") + feature.key);
        }

#ifdef SPK_WINDOWS_DATE_BACK
        const std::vector<uint16_t> clean_frame = render(engine.get(), session.get());
        const spk_status date_status = spk_set_params(session.get(),
            "{\"date_imprint_active\":true,\"date_imprint_style\":\"lcd\",\"date_imprint_text\":\"26\"}", nullptr);
        const std::string date_error = spk_last_error(engine.get());
        require(date_status == SPK_OK, "date back request was refused: " + date_error);
        const std::vector<uint16_t> dated_frame = render(engine.get(), session.get());
        require(clean_frame != dated_frame, "LCD date back did not change rendered pixels");
        require(spk_set_params(session.get(), "{\"date_imprint_active\":false}", nullptr) == SPK_OK,
                "date back could not be disabled after rendering");
        require(render(engine.get(), session.get()) != dated_frame, "disabling date back left the imprint enabled");
        before = params(session.get());

        std::vector<float> edge_pixels(32 * 21 * 3);
        for (size_t i = 0; i < edge_pixels.size(); ++i) edge_pixels[i] = 0.05f + float(i % 29) * 0.02f;
        const spk_image edge_input{edge_pixels.data(), 32, 21, 3};
        std::unique_ptr<spk_session, decltype(&spk_session_release)> edge_session(
            spk_open(engine.get(), &edge_input, "{\"grain_active\":false,\"glare_active\":false}", nullptr),
            spk_session_release);
        require(bool(edge_session), "overscan fixture open failed");
        spk_result frame{};
        require(spk_render(edge_session.get(), "full", &frame) == SPK_OK && frame.rgba16 &&
                frame.width == 32 && frame.height == 21, "base overscan fixture render failed");
        spk_result_free(&frame);
        const spk_status overscan_status = spk_set_params(edge_session.get(),
            "{\"overscan_active\":true,\"overscan_format\":\"135\",\"overscan_frame_seed\":5,"
            "\"date_imprint_active\":true,\"date_imprint_style\":\"lcd\",\"date_imprint_text\":\"26\"}", nullptr);
        require(overscan_status == SPK_OK, std::string("film edge request was refused: ") + spk_last_error(engine.get()));
        require(spk_render(edge_session.get(), "full", &frame) == SPK_OK && frame.rgba16 &&
                (frame.width > 32 || frame.height > 21), "film edge render did not expand the canvas");
        const auto whole_edge_width=frame.width,whole_edge_height=frame.height;
        spk_result_free(&frame);
        require(spk_set_params(edge_session.get(),"{\"geometry_crop_x\":0.25,\"geometry_crop_y\":0.25,\"geometry_crop_w\":0.5,\"geometry_crop_h\":0.5}",nullptr)==SPK_OK,"edge crop setup failed");
        require(spk_render(edge_session.get(),"full",&frame)==SPK_OK&&frame.rgba16,"cropped edge render failed");
        const auto cropped_edge_width=frame.width,cropped_edge_height=frame.height;
        spk_result_free(&frame);
        require(cropped_edge_width*10<=whole_edge_width*6&&cropped_edge_height*10<=whole_edge_height*6,
                "cropped film gate retained the full RAW pitch and an oversized border");
        require(spk_set_params(edge_session.get(),"{\"print_exposure\":1.1}",nullptr)==SPK_OK&&
                spk_reprint(edge_session.get(),"full",&frame)==SPK_OK&&frame.width==cropped_edge_width&&frame.height==cropped_edge_height,
                "cached-negative reprint changed the cropped border dimensions");
        spk_result_free(&frame);
        require(spk_set_params(edge_session.get(),"{\"geometry_crop_x\":0,\"geometry_crop_y\":0,\"geometry_crop_w\":1,\"geometry_crop_h\":1}",nullptr)==SPK_OK,"edge crop reset failed");
        require(spk_render(edge_session.get(),"full",&frame)==SPK_OK&&frame.width==whole_edge_width&&frame.height==whole_edge_height,
                "restoring full framing did not restore the original physical border");
        spk_result_free(&frame);
        char* geometry = nullptr;
        require(spk_overscan_geometry(edge_session.get(), &geometry) == SPK_OK && geometry &&
                std::string(geometry).find("\"valid\":true") != std::string::npos,
                "film edge did not produce valid geometry");
        spk_string_free(geometry);
        require(spk_set_params(edge_session.get(), "{\"overscan_active\":false,\"date_imprint_active\":false}", nullptr) == SPK_OK,
                "film edge and date back could not be disabled");
        require(spk_render(edge_session.get(), "full", &frame) == SPK_OK && frame.width == 32 && frame.height == 21,
                "disabling film edge did not restore the frame dimensions");
        spk_result_free(&frame);
#endif

        spk_result di{};
        char* di_json = nullptr;
        const spk_status di_status = spk_render_digital_intermediate(session.get(), &di, &di_json);
        const std::string di_error = spk_last_error(engine.get());
        const bool valid_di = di.texture && di.rgba16 && di.width == 32 && di.height == 16 &&
            di_json && std::string(di_json).find("cineon") != std::string::npos;
        spk_result_free(&di);
        spk_string_free(di_json);
        require(di_status == SPK_OK && valid_di, "Cineon DI render failed: " + di_error);
        require(params(session.get()) == before, "DI export mutated the session");
        render(engine.get(), session.get());

        std::unique_ptr<spk_session, decltype(&spk_session_release)> slide(
            spk_open(engine.get(), &input, "{\"film_stock\":\"fujifilm_provia_100f\"}", nullptr), spk_session_release);
        require(bool(slide), "slide open failed");
        spk_result slide_di{};
        char* slide_json = nullptr;
        require(spk_render_digital_intermediate(slide.get(), &slide_di, &slide_json) == SPK_ERR_USER &&
            !slide_di.texture && !slide_di.rgba16 && !slide_json, "slide film accepted a negative DI");

        const float* mask = nullptr;
        uint32_t width = 0, height = 0;
        require(spk_contrast_mask_field(session.get(), "full", &mask, &width, &height) == SPK_OK &&
                !mask && width == 0 && height == 0, "disabled contrast mask must have an empty field");

        // Both analysis and the resolved mapping run on the Vulkan backend.
        char* latitude_json = nullptr;
        const spk_status latitude_status = spk_scene_latitude(session.get(), nullptr, &latitude_json);
        const std::string latitude_error = spk_last_error(engine.get());
        const bool latitude_valid = latitude_json &&
            std::string(latitude_json).find("\"medium\"") != std::string::npos &&
            std::string(latitude_json).find("\"scene\"") != std::string::npos;
        spk_string_free(latitude_json);
        require(latitude_status == SPK_OK && latitude_valid, "latitude analysis failed: " + latitude_error);
        require(params(session.get()) == before, "latitude analysis mutated the session");
        require(spk_set_params(session.get(), "{\"scene_latitude_active\":true,\"scene_latitude_highlight_knee\":0,\"scene_latitude_highlight_room\":1}", nullptr) == SPK_OK,
            "latitude mapping was refused");
        render(engine.get(), session.get());
        std::cout << "Windows feature boundaries passed (unsupported effects, date back, Cineon/slide contract, latitude analysis/render)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
