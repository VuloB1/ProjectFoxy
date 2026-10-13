# Resize black-strip test (-Image <file> opens that picture first) (Windows). Grows the real window in 41 steps, repeated -Rounds times, and after each step (waiting
# -DelayMs) screenshots it and counts pure-black or pure-white pixels in the strip the step just added. 0 ms catches the very first frame;
# >= 5 ms is what a person would still see. Pick the renderer with QSG_RHI_BACKEND (d3d11 | opengl | software). Needs the
# window to fit the screen (it uses 20,0 and grows to about 2500 x 1320) and does not touch the mouse.
# Measured 2026-10-12 (2560x1440, native resize): d3d11 6.3M px at 0 ms and 85 k at 2 ms, ~20 from 5 ms; opengl ~40 even at 0 ms.
param([int]$DelayMs = 0, [int]$Rounds = 6, [string]$Image = "", [switch]$Drag)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public class R {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l,t,r,b; }
  // pure-black pixels in the window area after a resize, split into the right and bottom bands where a new strip would be
  public static int[] Shoot(IntPtr h, Bitmap bmp, int prevW, int prevH) {
    RECT r; GetWindowRect(h, out r);
    int w = r.r - r.l, ht = r.b - r.t;
    using (Graphics g = Graphics.FromImage(bmp)) g.CopyFromScreen(r.l, r.t, 0, 0, new Size(w, ht));
    BitmapData d = bmp.LockBits(new Rectangle(0, 0, w, ht), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    byte[] buf = new byte[d.Stride * ht];
    Marshal.Copy(d.Scan0, buf, 0, buf.Length);
    int right = 0, bottom = 0;
    for (int y = 40; y < ht - 1; y++) { int row = y * d.Stride;
      for (int x = 1; x < w - 1; x++) { int i = row + x * 4;
        if ((buf[i] < 3 && buf[i+1] < 3 && buf[i+2] < 3) || (buf[i] > 252 && buf[i+1] > 252 && buf[i+2] > 252)) { if (x >= prevW - 2) right++; else if (y >= prevH - 2) bottom++; } } }
    bmp.UnlockBits(d);
    return new int[] { right, bottom };
  }
}
'@
[R]::SetProcessDPIAware() | Out-Null
$p = Start-Process "C:\build\imageviewer\release\ProjectFoxy.exe" -ArgumentList $(if ($Image) { @("--new-window", $Image) } else { @("--new-window") }) -PassThru
Start-Sleep 6; $h = $p.MainWindowHandle
[R]::SetForegroundWindow($h) | Out-Null
$bmp = New-Object Drawing.Bitmap 2560,1440
$r = New-Object R+RECT; [R]::GetWindowRect($h,[ref]$r) | Out-Null
$x = 20; $y = 0; $flags = 0x0010 -bor 0x0004   # NOACTIVATE | NOZORDER
[R]::SetWindowPos($h,[IntPtr]::Zero,$x,$y,900,600,$flags) | Out-Null; Start-Sleep 1
$total = 0; $worst = 0; $frames = 0
for ($round = 0; $round -lt $Rounds; $round++) {
  foreach ($i in 0..40) {                       # grow in 40 quick steps
    $w = 900 + $i * 40; $hh = 600 + $i * 18; $pw = $w - 40; $ph = $hh - 18
    if ($Drag) {
      [R]::GetWindowRect($h,[ref]$r) | Out-Null
      if ($i -eq 0) { [R]::SetCursorPos($r.r - 2, $r.b - 2) | Out-Null; Start-Sleep -m 200; [R]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -m 100 }
      [R]::SetCursorPos($x + $w - 2, $y + $hh - 2) | Out-Null
    } else { [R]::SetWindowPos($h,[IntPtr]::Zero,$x,$y,$w,$hh,$flags) | Out-Null }
    if ($DelayMs -gt 0) { Start-Sleep -Milliseconds $DelayMs }
    $c = [R]::Shoot($h,$bmp,$pw,$ph); $n = $c[0] + $c[1]
    $total += $n; $frames++; if ($n -gt $worst) { $worst = $n }
  }
  if ($Drag) { [R]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
  [R]::SetWindowPos($h,[IntPtr]::Zero,$x,$y,900,600,$flags) | Out-Null; Start-Sleep -Milliseconds 300
}
Start-Sleep 1
$still = [R]::Shoot($h,$bmp,0,0); "still window, black px outside bands: $($still[0]+$still[1]) (should be 0)"
"frames=$frames  delay=${DelayMs}ms  black/white px in new strips: total=$total  worst frame=$worst"
Stop-Process $p -Force
