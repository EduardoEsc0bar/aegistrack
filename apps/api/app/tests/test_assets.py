def test_assets_returns_seeded_and_runtime_shape(client):
    response = client.get("/api/v1/assets")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) >= 3
    assert {"id", "name", "type", "status", "latitude", "longitude"} <= payload[0].keys()
