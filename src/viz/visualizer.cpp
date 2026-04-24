#include "viz/visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#if defined(AEGISTRACK_VIZ)
#include <SFML/Graphics.hpp>
#endif

namespace sensor_fusion::viz {

Visualizer::Visualizer(VizConfig cfg) : cfg_(cfg) {}

#if defined(AEGISTRACK_VIZ)
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool finite_xy(const std::array<double, 3>& pos) {
  return std::isfinite(pos[0]) && std::isfinite(pos[1]);
}

sf::Vector2f world_to_screen(const VizConfig& cfg, double x_m, double y_m) {
  const float sx = static_cast<float>(cfg.width * 0.5 + x_m / cfg.meters_per_pixel);
  const float sy = static_cast<float>(cfg.height * 0.5 - y_m / cfg.meters_per_pixel);
  return {sx, sy};
}

bool load_default_font(sf::Font& font) {
  static const std::vector<std::string> kCandidates = {
      "/System/Library/Fonts/Supplemental/Arial.ttf",
      "/System/Library/Fonts/Supplemental/Helvetica.ttc",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
      "/usr/local/share/fonts/DejaVuSans.ttf",
  };
  for (const auto& path : kCandidates) {
#if SFML_VERSION_MAJOR >= 3
    if (font.openFromFile(path)) {
#else
    if (font.loadFromFile(path)) {
#endif
      return true;
    }
  }
  return false;
}

void draw_covariance_ellipse(sf::RenderWindow& window,
                             const VizConfig& cfg,
                             const VizSnapshot::TrackViz& track) {
  Eigen::Matrix2d cov = track.P.block<2, 2>(0, 0);
  cov = 0.5 * (cov + cov.transpose());
  if (!cov.array().isFinite().all()) {
    return;
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> solver(cov);
  if (solver.info() != Eigen::Success) {
    return;
  }

  Eigen::Vector2d evals = solver.eigenvalues();
  evals(0) = std::max(0.0, evals(0));
  evals(1) = std::max(0.0, evals(1));
  if (!evals.array().isFinite().all()) {
    return;
  }

  constexpr double sigma = 2.0;
  double major = sigma * std::sqrt(std::max(evals(0), evals(1)));
  double minor = sigma * std::sqrt(std::min(evals(0), evals(1)));
  if (!std::isfinite(major) || !std::isfinite(minor) || major <= 1e-6 || minor <= 1e-6) {
    return;
  }

  Eigen::Vector2d major_vec =
      (evals(1) >= evals(0)) ? solver.eigenvectors().col(1) : solver.eigenvectors().col(0);
  const float angle_deg =
      static_cast<float>(std::atan2(major_vec(1), major_vec(0)) * 180.0 / kPi);

  sf::CircleShape ellipse(1.0f);
  ellipse.setPointCount(64);
  ellipse.setFillColor(sf::Color::Transparent);
  ellipse.setOutlineThickness(1.0f);
  ellipse.setOutlineColor(track.confirmed ? sf::Color(60, 220, 60) : sf::Color(240, 220, 60));
  ellipse.setOrigin(sf::Vector2f(1.0f, 1.0f));
  ellipse.setPosition(world_to_screen(cfg, track.pos[0], track.pos[1]));
  ellipse.setScale(sf::Vector2f(static_cast<float>(major / cfg.meters_per_pixel),
                                static_cast<float>(minor / cfg.meters_per_pixel)));
#if SFML_VERSION_MAJOR >= 3
  ellipse.setRotation(sf::degrees(angle_deg));
#else
  ellipse.setRotation(angle_deg);
#endif
  window.draw(ellipse);
}

}  // namespace
#endif

void Visualizer::run(std::function<bool(VizSnapshot& out)> get_snapshot) {
#if !defined(AEGISTRACK_VIZ)
  (void)get_snapshot;
  return;
#else
#if SFML_VERSION_MAJOR >= 3
  sf::RenderWindow window(
      sf::VideoMode(
          sf::Vector2u(static_cast<unsigned int>(std::max(1, cfg_.width)),
                       static_cast<unsigned int>(std::max(1, cfg_.height)))),
      "AegisTrack 2D Viz", sf::Style::Titlebar | sf::Style::Close);
#else
  sf::RenderWindow window(
      sf::VideoMode(static_cast<unsigned int>(std::max(1, cfg_.width)),
                    static_cast<unsigned int>(std::max(1, cfg_.height))),
      "AegisTrack 2D Viz", sf::Style::Titlebar | sf::Style::Close);
#endif
  window.setFramerateLimit(60);

  sf::Font font;
  const bool has_font = load_default_font(font);

  VizSnapshot snapshot;

  while (window.isOpen()) {
    #if SFML_VERSION_MAJOR >= 3
    while (const std::optional event = window.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        window.close();
      }
    }
    #else
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }
    #endif

    if (!get_snapshot(snapshot)) {
      window.close();
      break;
    }

    window.clear(sf::Color::Black);

    const float protected_px =
        static_cast<float>(cfg_.protected_zone_radius_m / std::max(1e-9, cfg_.meters_per_pixel));
    sf::CircleShape zone(protected_px);
    zone.setFillColor(sf::Color::Transparent);
    zone.setOutlineThickness(1.0f);
    zone.setOutlineColor(sf::Color(120, 120, 120));
    zone.setOrigin(sf::Vector2f(protected_px, protected_px));
    zone.setPosition(world_to_screen(cfg_, 0.0, 0.0));
    window.draw(zone);

    if (cfg_.show_truth) {
      for (const auto& pos : snapshot.truth_positions) {
        if (!finite_xy(pos)) {
          continue;
        }
        sf::CircleShape dot(3.0f);
        dot.setFillColor(sf::Color::White);
        dot.setOrigin(sf::Vector2f(3.0f, 3.0f));
        dot.setPosition(world_to_screen(cfg_, pos[0], pos[1]));
        window.draw(dot);
      }
    }

    if (cfg_.show_tracks) {
      for (const auto& track : snapshot.tracks) {
        if (!finite_xy(track.pos)) {
          continue;
        }
        sf::CircleShape dot(4.0f);
        dot.setFillColor(track.confirmed ? sf::Color(60, 220, 60) : sf::Color(240, 220, 60));
        dot.setOrigin(sf::Vector2f(4.0f, 4.0f));
        const sf::Vector2f center = world_to_screen(cfg_, track.pos[0], track.pos[1]);
        dot.setPosition(center);
        window.draw(dot);

        if (cfg_.show_covariance) {
          draw_covariance_ellipse(window, cfg_, track);
        }

        if (has_font) {
#if SFML_VERSION_MAJOR >= 3
          sf::Text label(font, "", 12);
#else
          sf::Text label;
          label.setFont(font);
          label.setCharacterSize(12);
#endif
          label.setFillColor(sf::Color(220, 220, 220));
          label.setString(std::to_string(track.id));
          label.setPosition(sf::Vector2f(center.x + 6.0f, center.y - 6.0f));
          window.draw(label);
        }
      }
    }

    if (cfg_.show_interceptors) {
      for (const auto& interceptor : snapshot.interceptors) {
        if (!finite_xy(interceptor.pos)) {
          continue;
        }
        sf::CircleShape dot(6.0f);
        dot.setFillColor(sf::Color(60, 120, 255));
        dot.setOrigin(sf::Vector2f(6.0f, 6.0f));
        dot.setPosition(world_to_screen(cfg_, interceptor.pos[0], interceptor.pos[1]));
        window.draw(dot);
      }
    }

    if (cfg_.show_engagements) {
      for (const auto& line : snapshot.engagement_lines) {
        if (!finite_xy(line.first) || !finite_xy(line.second)) {
          continue;
        }
        const sf::Vector2f a = world_to_screen(cfg_, line.first[0], line.first[1]);
        const sf::Vector2f b = world_to_screen(cfg_, line.second[0], line.second[1]);
        sf::Vertex vertices[2] = {{a, sf::Color(220, 60, 60)}, {b, sf::Color(220, 60, 60)}};
#if SFML_VERSION_MAJOR >= 3
        window.draw(vertices, 2, sf::PrimitiveType::Lines);
#else
        window.draw(vertices, 2, sf::Lines);
#endif
      }
    }

    window.display();
  }
#endif
}

}  // namespace sensor_fusion::viz
