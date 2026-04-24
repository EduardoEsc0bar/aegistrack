from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.assets import TrackSummary
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("", response_model=list[TrackSummary])
def list_tracks(db: Session = Depends(get_db)) -> list[TrackSummary]:
    service = MissionControlService(db)
    return [TrackSummary.model_validate(track) for track in service.list_tracks()]
