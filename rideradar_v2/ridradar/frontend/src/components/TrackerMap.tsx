import { useEffect, useRef, useState } from "react";
import "leaflet/dist/leaflet.css";
import type L from "leaflet";
import type { TrackerLocation } from "@/types/telemetry";

export const SURAT_CENTER: [number, number] = [21.1702, 72.8311];

export function TrackerMap({ location }: { location: TrackerLocation | null }) {
  const mapNode = useRef<HTMLDivElement>(null);
  const mapRef = useRef<L.Map | null>(null);
  const markerRef = useRef<L.Marker | null>(null);
  const [mapReady, setMapReady] = useState(false);

  useEffect(() => {
    let disposed = false;
    void import("leaflet").then(({ default: leaflet }) => {
      if (disposed || !mapNode.current || mapRef.current) return;
      const initialPos = location ? [location.latitude, location.longitude] as [number, number] : SURAT_CENTER;
      const map = leaflet.map(mapNode.current, { zoomControl: true }).setView(initialPos, 13);
      leaflet
        .tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
          attribution: "© OpenStreetMap contributors",
        })
        .addTo(map);
      mapRef.current = map;
      setMapReady(true);
      window.setTimeout(() => map.invalidateSize(), 200);
    });

    return () => {
      disposed = true;
      mapRef.current?.remove();
      mapRef.current = null;
    };
  }, []);

  useEffect(() => {
    if (!mapReady || !location) return;
    const point: [number, number] = [location.latitude, location.longitude];
    void import("leaflet").then(({ default: leaflet }) => {
      const map = mapRef.current;
      if (!map) return;

      const popupHtml = `
        <div style="font-family: sans-serif; font-size: 13px; line-height: 1.4;">
          <strong>RideRadar Tracker (${location.id})</strong><br/>
          Lat: ${location.latitude.toFixed(6)}<br/>
          Lng: ${location.longitude.toFixed(6)}
          ${location.speed != null ? `<br/>Speed: <b>${location.speed} km/h</b>` : ""}
          ${location.satellites != null ? `<br/>Satellites: <b>${location.satellites}</b>` : ""}
        </div>
      `;

      if (!markerRef.current) {
        markerRef.current = leaflet
          .marker(point, {
            icon: leaflet.divIcon({
              className: "tracker-marker",
              html: "<span></span>",
              iconSize: [32, 32],
              iconAnchor: [16, 16],
            }),
          })
          .addTo(map)
          .bindPopup(popupHtml);
        map.setView(point, 14, { animate: true });
      } else {
        markerRef.current.setLatLng(point);
        markerRef.current.setPopupContent(popupHtml);
        map.panTo(point, { animate: true });
      }
    });
  }, [location, mapReady]);

  return (
    <div className="map-frame">
      <div ref={mapNode} className="map-canvas" />
      <div className="map-caption">
        <span className="map-key" /> Surat operations view
      </div>
      {location && (
        <button
          className="recenter-button"
          onClick={() => mapRef.current?.flyTo([location.latitude, location.longitude], 15)}
          type="button"
        >
          Recenter on tracker
        </button>
      )}
    </div>
  );
}