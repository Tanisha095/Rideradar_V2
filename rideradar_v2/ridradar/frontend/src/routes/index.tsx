import { createFileRoute } from "@tanstack/react-router";
import { TrackerPage } from "./tracking";

export const Route = createFileRoute("/")({
  component: TrackerPage,
});