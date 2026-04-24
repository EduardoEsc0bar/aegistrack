from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.schemas.assets import AssetSummary
from app.services.mission_control import MissionControlService

router = APIRouter()


@router.get("", response_model=list[AssetSummary])
def list_assets(db: Session = Depends(get_db)) -> list[AssetSummary]:
    service = MissionControlService(db)
    return [AssetSummary.model_validate(asset) for asset in service.list_assets()]


@router.get("/{asset_id}", response_model=AssetSummary)
def get_asset(asset_id: str, db: Session = Depends(get_db)) -> AssetSummary:
    service = MissionControlService(db)
    asset = service.get_asset(asset_id)
    if asset is None:
        raise HTTPException(status_code=404, detail="Asset not found")
    return AssetSummary.model_validate(asset)
