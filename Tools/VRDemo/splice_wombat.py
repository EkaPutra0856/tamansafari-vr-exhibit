# Swaps the wombat part of a previous recording for a new one: frames before marker CUT come from the old run, the rest from
# the new run (both from the same script, so the runs match up to that marker; the frame difference at the cut is printed).
# Hard links into Saved/DemoVideo/Frames_spliced, then encodes ZooVR_Demo.mp4 + timestamps.md like make_mp4.py.
# Usage: python splice_wombat.py [--old Frames_run1] [--old-markers markers_run1.txt] [--cut rute_ke_wombat]
import argparse, os, shutil, subprocess
import numpy as np
from PIL import Image
ROOT = r"D:\Projek UE\ZooEka2VR\Saved\DemoVideo"
FFMPEG = r"C:\ffmpeg\bin\ffmpeg.exe"
FPS = 30
ap = argparse.ArgumentParser()
ap.add_argument("--old", default="Frames_run1")
ap.add_argument("--old-markers", default="markers_run1.txt")
ap.add_argument("--new", default="Frames")
ap.add_argument("--new-markers", default="markers.txt")
ap.add_argument("--cut", default="rute_ke_wombat")
ap.add_argument("--out", default="ZooVR_Demo.mp4")
a = ap.parse_args()

def markers(name):
    rows = []
    for row in open(os.path.join(ROOT, name), encoding="utf-8"):
        p = row.rstrip("\n").split("\t")
        if len(p) >= 4: rows.append((int(p[0]), p[3]))
    return rows

def frame(d, i): return os.path.join(ROOT, d, "frame_%06d.jpg" % i)

old_m, new_m = markers(a.old_markers), markers(a.new_markers)
old_cut = next(f for f, n in old_m if n == a.cut)
new_cut = next(f for f, n in new_m if n == a.cut)
diff = np.abs(np.asarray(Image.open(frame(a.old, old_cut)), np.float32) - np.asarray(Image.open(frame(a.new, new_cut)), np.float32)).mean()
print(f"cut '{a.cut}': old frame {old_cut}, new frame {new_cut}, mean abs diff {diff:.1f}/255")

out_dir = os.path.join(ROOT, "Frames_spliced")
shutil.rmtree(out_dir, ignore_errors=True)
os.makedirs(out_dir)
n = 0
for i in range(old_cut):
    os.link(frame(a.old, i), os.path.join(out_dir, "frame_%06d.jpg" % n)); n += 1
new_count = len([f for f in os.listdir(os.path.join(ROOT, a.new)) if f.startswith("frame_")])
for i in range(new_cut, new_count):
    os.link(frame(a.new, i), os.path.join(out_dir, "frame_%06d.jpg" % n)); n += 1
shift = old_cut - new_cut
rows = [(f, nm) for f, nm in old_m if f < old_cut] + [(f + shift, nm) for f, nm in new_m if f >= new_cut]
print(f"{n} frames = {n / FPS:.1f} s")

out = os.path.join(ROOT, a.out)
subprocess.check_call([FFMPEG, "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", os.path.join(out_dir, "frame_%06d.jpg"),
    "-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p", "-r", str(FPS), "-movflags", "+faststart", out])
lines = ["# ZooVR demo timestamps", "", f"Video: {a.out}, {n} frames @ {FPS} fps = {n // FPS // 60}:{n // FPS % 60:02d}",
    f"Bagian wombat (mulai '{a.cut}') direkam ulang; sebelum itu dari rekaman sebelumnya.", "", "| Waktu | Frame | Fitur |", "|---|---|---|"]
for f, nm in rows:
    s = f / FPS
    lines.append(f"| {int(s // 60)}:{int(s % 60):02d} | {f} | {nm} |")
open(os.path.join(ROOT, "timestamps.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines))
