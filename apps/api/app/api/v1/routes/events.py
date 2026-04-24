from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.events import EventSummary
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("", response_model=list[EventSummary])
def list_events(db: Session = Depends(get_db)) -> list[EventSummary]:
    service = MissionControlService(db)
    return [EventSummary.model_validate(event) for event in service.list_events()]
