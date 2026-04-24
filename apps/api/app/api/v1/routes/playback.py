from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.playback import PlaybackActionResponse, PlaybackStartRequest, PlaybackState
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("/state", response_model=PlaybackState)
def get_playback_state(db: Session = Depends(get_db)) -> PlaybackState:
    return PlaybackState.model_validate(MissionControlService(db).playback_state())


@router.post("/start", response_model=PlaybackActionResponse)
def start_playback(payload: PlaybackStartRequest, db: Session = Depends(get_db)) -> PlaybackActionResponse:
    state = MissionControlService(db).start_playback(payload.start_time, payload.end_time)
    return PlaybackActionResponse(state=PlaybackState.model_validate(state))


@router.post("/stop", response_model=PlaybackActionResponse)
def stop_playback(db: Session = Depends(get_db)) -> PlaybackActionResponse:
    state = MissionControlService(db).stop_playback()
    return PlaybackActionResponse(state=PlaybackState.model_validate(state))
