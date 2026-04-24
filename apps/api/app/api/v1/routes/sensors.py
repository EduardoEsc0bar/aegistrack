from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.assets import SensorSummary
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("", response_model=list[SensorSummary])
def list_sensors(db: Session = Depends(get_db)) -> list[SensorSummary]:
    service = MissionControlService(db)
    return [SensorSummary.model_validate(sensor) for sensor in service.list_sensors()]
