export type TrackerLocation = {
  id: string;
  latitude: number;
  longitude: number;
  receivedAt: string;
  speed?: number | null;
  satellites?: number | null;
  heading?: number | null;
};
export type TrackerStatus = "CONNECTING" | "LIVE" | "OFFLINE";