def test_events_returns_timeline_entries(client):
    response = client.get("/api/v1/events")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) >= 2
    assert {"id", "title", "severity", "occurred_at"} <= payload[0].keys()
