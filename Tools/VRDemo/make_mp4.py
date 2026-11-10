# Encodes Saved/DemoVideo/Frames/frame_%06d.png (30 fps) into Saved/DemoVideo/ZooVR_Demo.mp4 (H.264) and writes
# Saved/DemoVideo/timestamps.md from markers.txt (frame, seconds, game time, name).
# Usage: python make_mp4.py [--audio <wav>] [--out <name.mp4>] [--crf 18]
import argparse, os, subprocess, sys, glob
ROOT = r"D:\Projek UE\ZooEka2VR\Saved\DemoVideo"
FFMPEG = r"C:\ffmpeg\bin\ffmpeg.exe"
FPS = 30
ap = argparse.ArgumentParser()
ap.add_argument("--audio", default=None)
ap.add_argument("--out", default="ZooVR_Demo.mp4")
ap.add_argument("--crf", default="18")
ap.add_argument("--frames", default=os.path.join(ROOT, "Frames"))
a = ap.parse_args()
frames = sorted(glob.glob(os.path.join(a.frames, "frame_*.jpg")))
if not frames:
    sys.exit("no frames in " + a.frames)
n = len(frames)
print(f"{n} frames = {n / FPS:.1f} s")
out = os.path.join(ROOT, a.out)
cmd = [FFMPEG, "-y", "-framerate", str(FPS), "-i", os.path.join(a.frames, "frame_%06d.jpg")]
if a.audio:
    cmd += ["-i", a.audio, "-shortest", "-c:a", "aac", "-b:a", "192k"]
cmd += ["-c:v", "libx264", "-preset", "medium", "-crf", a.crf, "-pix_fmt", "yuv420p", "-r", str(FPS), "-movflags", "+faststart", out]
print(" ".join(cmd))
subprocess.check_call(cmd)
# Timestamps
mk = os.path.join(ROOT, "markers.txt")
lines = ["# ZooVR demo timestamps", "", f"Video: {os.path.basename(out)}, {n} frames @ {FPS} fps = {n // FPS // 60}:{n // FPS % 60:02d}", "", "| Waktu | Frame | Fitur |", "|---|---|---|"]
if os.path.exists(mk):
    for row in open(mk, encoding="utf-8"):
        p = row.rstrip("\n").split("\t")
        if len(p) < 4: continue
        f = int(p[0]); s = f / FPS
        lines.append(f"| {int(s // 60)}:{int(s % 60):02d} | {f} | {p[3]} |")
open(os.path.join(ROOT, "timestamps.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines))
