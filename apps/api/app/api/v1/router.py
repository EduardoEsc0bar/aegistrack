from fastapi import APIRouter

from app.api.v1.routes import assets, events, health, playback, sensors, telemetry, tracks

api_router = APIRouter()
api_router.include_router(health.router, tags=["health"])
api_router.include_router(assets.router, prefix="/assets", tags=["assets"])
api_router.include_router(sensors.router, prefix="/sensors", tags=["sensors"])
api_router.include_router(events.router, prefix="/events", tags=["events"])
api_router.include_router(tracks.router, prefix="/tracks", tags=["tracks"])
api_router.include_router(telemetry.router, prefix="/telemetry", tags=["telemetry"])
api_router.include_router(playback.router, prefix="/playback", tags=["playback"])
