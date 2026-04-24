from __future__ import annotations

from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="AEGISTRACK_API_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    env: str = "development"
    app_name: str = "AegisTrack Mission Control API"
    database_url: str = "sqlite+pysqlite:///./aegistrack_mission_control.db"
    redis_url: str = "redis://localhost:6379/0"
    log_path: str = "/workspace/logs/mission_control.jsonl"
    profile_path: str = "/workspace/results/profile.json"
    allowed_origins: str = "http://localhost:3000"
    enable_file_bridge: bool = True
    auto_seed: bool = True
    anchor_latitude: float = 26.2048
    anchor_longitude: float = 56.2708
    telemetry_interval_ms: int = 1000


@lru_cache(maxsize=1)
def get_settings() -> Settings:
    return Settings()
