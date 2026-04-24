from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.telemetry import TelemetryUpdate
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("/latest", response_model=list[TelemetryUpdate])
def latest_telemetry(db: Session = Depends(get_db)) -> list[TelemetryUpdate]:
    service = MissionControlService(db)
    return [TelemetryUpdate.model_validate(item) for item in service.latest_telemetry()]
