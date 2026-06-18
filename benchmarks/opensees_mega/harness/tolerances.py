"""Tolerance and classification helpers for the OpenSees mega benchmark."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Tolerance:
    disp: float
    force: float
    freq: float
    analytic: float


TOLERANCES = {
    "beam": Tolerance(disp=1.0e-8, force=1.0e-8, freq=1.0e-4, analytic=1.0e-6),
    "shell": Tolerance(disp=1.0e-6, force=1.0e-4, freq=1.0e-3, analytic=3.0e-2),
    "curved_shell": Tolerance(disp=1.0e-3, force=1.0e-2, freq=1.0e-2, analytic=5.0e-2),
    "pdelta": Tolerance(disp=1.0e-2, force=1.0e-2, freq=1.0e-2, analytic=1.0e-2),
    "known_gap": Tolerance(disp=1.0, force=1.0, freq=1.0, analytic=1.0),
}


def classify(max_rel: float | None, tolerance: float, *, known: bool = False, failed: bool = False) -> str:
    if failed:
        return "CRITICAL"
    if known:
        return "KNOWN"
    if max_rel is None:
        return "KNOWN"
    if max_rel > 0.50:
        return "CRITICAL"
    if max_rel > 0.05:
        return "MAJOR"
    if max_rel > tolerance:
        return "MINOR"
    return "MINOR"


def worst_verdict(verdicts: list[str]) -> str:
    order = {"CRITICAL": 4, "MAJOR": 3, "MINOR": 2, "KNOWN": 1, "PASS": 0, "SKIPPED": 0}
    return max(verdicts, key=lambda v: order.get(v, 0)) if verdicts else "SKIPPED"
