from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from app.api.v1.router import api_router
from app.core.config import get_settings
from app.core.database import SessionLocal, engine
from app.db.base import Base
from app.schemas.telemetry import TelemetryUpdate
from app.services.seed import seed_database
from app.services.telemetry_stream import TelemetryStreamService

settings = get_settings()


@asynccontextmanager
async def lifespan(_: FastAPI):
    Base.metadata.create_all(bind=engine)
    if settings.auto_seed:
        with SessionLocal() as session:
            seed_database(session)
    yield


app = FastAPI(title=settings.app_name, version="0.1.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=[origin.strip() for origin in settings.allowed_origins.split(",") if origin.strip()],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)
app.include_router(api_router)
app.include_router(api_router, prefix="/api/v1")


@app.websocket("/ws/telemetry")
async def telemetry_socket(websocket: WebSocket) -> None:
    await websocket.accept()
    stream_service = TelemetryStreamService()
    try:
        async for payload in stream_service.stream(settings.telemetry_interval_ms):
            await websocket.send_json(TelemetryUpdate.model_validate(payload).model_dump(mode="json"))
    except WebSocketDisconnect:
        return
