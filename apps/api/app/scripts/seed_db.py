from app.core.database import Base, SessionLocal, engine
from app.services.seed import seed_database


def main() -> None:
    Base.metadata.create_all(bind=engine)
    with SessionLocal() as session:
        seed_database(session)


if __name__ == "__main__":
    main()
