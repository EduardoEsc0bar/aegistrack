from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.health import HealthResponse
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("/health", response_model=HealthResponse)
def get_health(db: Session = Depends(get_db)) -> HealthResponse:
    return HealthResponse.model_validate(MissionControlService(db).health())
