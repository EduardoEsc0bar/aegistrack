"use client";

import { useEffect, useRef, useState } from "react";

import { Asset, Sensor, Track } from "@/lib/types";
import { useUiStore } from "@/store/ui-store";

type Props = {
  assets: Asset[];
  sensors: Sensor[];
  tracks: Track[];
};

const SECTOR_CENTER = { lat: 25.05, lon: 55.05 };

export function GlobeViewer({ assets, sensors, tracks }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const viewerRef = useRef<any>(null);
  const cesiumRef = useRef<any>(null);
  const [viewerReady, setViewerReady] = useState(false);
  const layers = useUiStore((state) => state.layers);
  const setSelectedEntityId = useUiStore((state) => state.setSelectedEntityId);

  function metersToLat(meters: number) {
    return meters / 111_320;
  }

  function metersToLon(meters: number, lat = SECTOR_CENTER.lat) {
    return meters / (111_320 * Math.cos((lat * Math.PI) / 180));
  }

  function classifyAssetTone(asset: Asset) {
    if (asset.type === "interceptor") return "#5f8cff";
    if (asset.type.includes("satellite") || asset.type.includes("orbital")) return "#4ad7d1";
    if (asset.status === "standby" || asset.status === "idle") return "#8ea5bb";
    return "#7ee787";
  }

  useEffect(() => {
    let mounted = true;

    async function mountViewer() {
      (window as any).CESIUM_BASE_URL = process.env.NEXT_PUBLIC_CESIUM_BASE_URL ?? "/cesium";

      const Cesium = await import("cesium");
      if (!mounted || !containerRef.current) return;
      cesiumRef.current = Cesium;

      const viewer = new Cesium.Viewer(containerRef.current, {
        animation: false,
        baseLayer: false,
        baseLayerPicker: false,
        geocoder: false,
        homeButton: true,
        timeline: false,
        navigationHelpButton: false,
        sceneModePicker: false,
        fullscreenButton: false,
        infoBox: false,
        selectionIndicator: false
      });

      viewer.scene.globe.baseColor = Cesium.Color.fromCssColorString("#101f2b");
      viewer.scene.skyAtmosphere.show = false;
      viewer.scene.fog.enabled = false;
      viewer.scene.globe.enableLighting = false;
      viewer.scene.screenSpaceCameraController.minimumZoomDistance = 20_000;
      viewer.scene.screenSpaceCameraController.maximumZoomDistance = 8_000_000;
      viewer.camera.flyTo({
        destination: Cesium.Cartesian3.fromDegrees(SECTOR_CENTER.lon, SECTOR_CENTER.lat, 920_000),
        orientation: {
          heading: Cesium.Math.toRadians(18),
          pitch: Cesium.Math.toRadians(-72),
          roll: 0
        },
        duration: 0
      });

      const handler = new Cesium.ScreenSpaceEventHandler(viewer.scene.canvas);
      handler.setInputAction((movement: any) => {
        const picked = viewer.scene.pick(movement.position);
        if (picked?.id?.id) {
          setSelectedEntityId(String(picked.id.id));
        }
      }, Cesium.ScreenSpaceEventType.LEFT_CLICK);

      viewerRef.current = viewer;
      setViewerReady(true);
    }

    mountViewer();

    return () => {
      mounted = false;
      if (viewerRef.current) {
        viewerRef.current.destroy();
        viewerRef.current = null;
      }
      setViewerReady(false);
    };
  }, [setSelectedEntityId]);

  useEffect(() => {
    const viewer = viewerRef.current;
    const Cesium = cesiumRef.current;
    if (!viewerReady || !viewer || !Cesium) return;
    viewer.entities.removeAll();

    const addSectorGrid = () => {
      const gridColor = Cesium.Color.fromCssColorString("#284057").withAlpha(0.55);
      const ringColor = Cesium.Color.fromCssColorString("#4ad7d1").withAlpha(0.22);
      const axisColor = Cesium.Color.fromCssColorString("#4ad7d1").withAlpha(0.45);
      const extentMeters = 160_000;
      const stepMeters = 40_000;

      viewer.entities.add({
        id: "sector-fill",
        rectangle: {
          coordinates: Cesium.Rectangle.fromDegrees(
            SECTOR_CENTER.lon - metersToLon(extentMeters),
            SECTOR_CENTER.lat - metersToLat(extentMeters),
            SECTOR_CENTER.lon + metersToLon(extentMeters),
            SECTOR_CENTER.lat + metersToLat(extentMeters)
          ),
          material: Cesium.Color.fromCssColorString("#0f1c2a").withAlpha(0.36),
          height: 750
        }
      });

      for (let offset = -extentMeters; offset <= extentMeters; offset += stepMeters) {
        viewer.entities.add({
          id: `grid-lat-${offset}`,
          polyline: {
            positions: Cesium.Cartesian3.fromDegreesArrayHeights([
              SECTOR_CENTER.lon - metersToLon(extentMeters),
              SECTOR_CENTER.lat + metersToLat(offset),
              1_500,
              SECTOR_CENTER.lon + metersToLon(extentMeters),
              SECTOR_CENTER.lat + metersToLat(offset),
              1_500
            ]),
            width: offset === 0 ? 1.8 : 0.8,
            material: offset === 0 ? axisColor : gridColor
          }
        });
        viewer.entities.add({
          id: `grid-lon-${offset}`,
          polyline: {
            positions: Cesium.Cartesian3.fromDegreesArrayHeights([
              SECTOR_CENTER.lon + metersToLon(offset),
              SECTOR_CENTER.lat - metersToLat(extentMeters),
              1_500,
              SECTOR_CENTER.lon + metersToLon(offset),
              SECTOR_CENTER.lat + metersToLat(extentMeters),
              1_500
            ]),
            width: offset === 0 ? 1.8 : 0.8,
            material: offset === 0 ? axisColor : gridColor
          }
        });
      }

      [50_000, 100_000, 150_000].forEach((radius) => {
        viewer.entities.add({
          id: `sector-ring-${radius}`,
          position: Cesium.Cartesian3.fromDegrees(SECTOR_CENTER.lon, SECTOR_CENTER.lat, 2_000),
          ellipse: {
            semiMajorAxis: radius,
            semiMinorAxis: radius,
            material: Cesium.Color.TRANSPARENT,
            outline: true,
            outlineColor: ringColor,
            height: 2_000
          }
        });
      });

      viewer.entities.add({
        id: "sector-label",
        position: Cesium.Cartesian3.fromDegrees(SECTOR_CENTER.lon - 0.82, SECTOR_CENTER.lat + 1.42, 8_000),
        label: {
          text: "AEGIS OPS SECTOR / LIVE TRACK PICTURE",
          font: "12px ui-monospace, SFMono-Regular, monospace",
          fillColor: Cesium.Color.fromCssColorString("#4ad7d1"),
          outlineColor: Cesium.Color.BLACK,
          outlineWidth: 3,
          style: Cesium.LabelStyle.FILL_AND_OUTLINE,
          disableDepthTestDistance: Number.POSITIVE_INFINITY
        }
      });
    };

    addSectorGrid();

    if (layers.tracks) {
      assets.forEach((asset) => {
        const color = Cesium.Color.fromCssColorString(classifyAssetTone(asset));
        viewer.entities.add({
          id: asset.id,
          position: Cesium.Cartesian3.fromDegrees(
            asset.longitude,
            asset.latitude,
            Math.max(asset.altitude, 2_500)
          ),
          point: {
            pixelSize: asset.type === "interceptor" ? 12 : 9,
            color,
            outlineColor: Cesium.Color.fromCssColorString("#07111d"),
            outlineWidth: 2,
            disableDepthTestDistance: Number.POSITIVE_INFINITY
          },
          label: {
            text: asset.name.replace("asset-", "").slice(0, 18),
            font: "10px ui-monospace, SFMono-Regular, monospace",
            fillColor: Cesium.Color.WHITE,
            outlineColor: Cesium.Color.BLACK,
            outlineWidth: 3,
            style: Cesium.LabelStyle.FILL_AND_OUTLINE,
            pixelOffset: new Cesium.Cartesian2(0, -18),
            scale: 0.66,
            disableDepthTestDistance: Number.POSITIVE_INFINITY,
            distanceDisplayCondition: new Cesium.DistanceDisplayCondition(0, 1_900_000)
          }
        });
      });
    }

    if (layers.predictedPaths) {
      tracks.forEach((track) => {
        const positions = track.predicted_path.map((point) =>
          Cesium.Cartesian3.fromDegrees(point.lon, point.lat, point.alt)
        );
        viewer.entities.add({
          id: `${track.id}-path`,
          polyline: {
            positions,
            width: 2.5,
            material: Cesium.Color.fromCssColorString("#ffb65c").withAlpha(0.85),
            clampToGround: false
          }
        });
      });
    }

    if (layers.sensors) {
      sensors.forEach((sensor) => {
        viewer.entities.add({
          id: sensor.id,
          position: Cesium.Cartesian3.fromDegrees(sensor.longitude, sensor.latitude, 4_000),
          point: {
            pixelSize: 8,
            color: Cesium.Color.fromCssColorString("#ffb65c"),
            outlineColor: Cesium.Color.fromCssColorString("#07111d"),
            outlineWidth: 2,
            disableDepthTestDistance: Number.POSITIVE_INFINITY
          },
          label: {
            text: sensor.name.replace("Ground ", "").replace("Sector ", "").slice(0, 16),
            font: "10px ui-monospace, SFMono-Regular, monospace",
            fillColor: Cesium.Color.fromCssColorString("#ffb65c"),
            outlineColor: Cesium.Color.BLACK,
            outlineWidth: 3,
            style: Cesium.LabelStyle.FILL_AND_OUTLINE,
            pixelOffset: new Cesium.Cartesian2(0, 18),
            scale: 0.62,
            disableDepthTestDistance: Number.POSITIVE_INFINITY,
            distanceDisplayCondition: new Cesium.DistanceDisplayCondition(0, 1_500_000)
          }
        });
        if (layers.coverageCones) {
          const displayRange = Math.min(sensor.range_km, 90) * 1000;
          viewer.entities.add({
            id: `${sensor.id}-coverage`,
            position: Cesium.Cartesian3.fromDegrees(sensor.longitude, sensor.latitude, 0),
            ellipse: {
              semiMajorAxis: displayRange,
              semiMinorAxis: displayRange,
              material: Cesium.Color.fromCssColorString("#ffb65c").withAlpha(0.025),
              outline: true,
              outlineColor: Cesium.Color.fromCssColorString("#ffb65c").withAlpha(0.22)
            }
          });
        }
      });
    }

    if (assets.length > 0 || sensors.length > 0) {
      viewer.camera.flyTo({
        destination: Cesium.Cartesian3.fromDegrees(SECTOR_CENTER.lon, SECTOR_CENTER.lat, 920_000),
        orientation: {
          heading: Cesium.Math.toRadians(18),
          pitch: Cesium.Math.toRadians(-72),
          roll: 0
        },
        duration: 0.8
      });
    }
  }, [assets, sensors, tracks, layers, viewerReady]);

  return (
    <div className="relative overflow-hidden rounded-2xl border border-line bg-ink">
      <div ref={containerRef} className="h-[70vh] w-full" />
      <div className="pointer-events-none absolute left-4 top-4 rounded-xl border border-signal/20 bg-ink/70 px-4 py-3 shadow-panel backdrop-blur">
        <p className="text-[10px] font-semibold uppercase tracking-[0.28em] text-signal">AegisTrack Geo</p>
        <p className="mt-1 text-xs text-muted">
          {assets.length} assets / {sensors.length} sensors / {tracks.length} tracks
        </p>
      </div>
      <div className="pointer-events-none absolute bottom-4 right-4 rounded-xl border border-white/10 bg-ink/70 px-4 py-2 text-[10px] uppercase tracking-[0.22em] text-muted backdrop-blur">
        Sector Center {SECTOR_CENTER.lat.toFixed(2)}, {SECTOR_CENTER.lon.toFixed(2)}
      </div>
    </div>
  );
}
