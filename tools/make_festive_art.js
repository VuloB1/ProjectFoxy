// Draws the holiday decorations (resources/festive/*.svg) that qml/FestiveDecor.qml shows over the window.
//
//   node tools/make_festive_art.js
//
// For each holiday it writes <key>-garland.svg (a strip that is tiled along the top edge: it starts and ends on the
// same height, so the tiles join) and <key>-hero.svg (the piece for the corner of the thumbnail bar), plus
// halloween-web.svg. Only plain SVG that Qt's renderer understands (no clip paths, no gradients, no text): the holes
// are real holes (even-odd paths), so the art sits on any theme. Run it again after changing a drawing and commit the
// result; the program never runs this script.
const fs = require('fs');
const path = require('path');

const OUT = path.join(__dirname, '..', 'resources', 'festive');
fs.mkdirSync(OUT, { recursive: true });

// ------------------------------------------------------------------------------------------------ helpers
const f1 = n => n.toFixed(1);
const ring = (n, R, r, f) => { let s = ''; for (let i = 0; i < n; i++) { const a = i * 2 * Math.PI / n; s += `<circle cx="${f1(Math.cos(a) * R)}" cy="${f1(Math.sin(a) * R)}" r="${r}" fill="${f}"/>`; } return s; };
const ringE = (n, R, rx, ry, f) => { let s = ''; for (let i = 0; i < n; i++) s += `<ellipse cy="${-R}" rx="${rx}" ry="${ry}" fill="${f}" transform="rotate(${(i * 360 / n).toFixed(1)})"/>`; return s; };
const rot = (n, fn) => { let s = ''; for (let i = 0; i < n; i++) s += `<g transform="rotate(${(i * 360 / n).toFixed(2)})">${fn(i)}</g>`; return s; };
const stars = (c, R = 9, r = 3.6) => { let p = ''; for (let i = 0; i < 10; i++) { const q = i % 2 ? r : R, a = -Math.PI / 2 + i * Math.PI / 5; p += `${f1(Math.cos(a) * q)},${f1(Math.sin(a) * q)} `; } return `<polygon points="${p}" fill="${c}"/>`; };
const piece = (x, y, w, h, c, r) => `<rect x="${x - w / 2}" y="${y - h / 2}" width="${w}" height="${h}" fill="${c}" transform="rotate(${r} ${x} ${y})"/>`;
const circ = (cx, cy, r) => `M${cx - r} ${cy} a${r} ${r} 0 1 0 ${2 * r} 0 a${r} ${r} 0 1 0 ${-2 * r} 0Z`; // for even-odd holes
const svg = (vb, body) => { const [x, y, w, h] = vb.split(' '); return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${vb}" width="${w}" height="${h}">${body}</svg>\n`; };
const petal = (len, w, c) => `<path d="M0 0 C${-w} ${-len * .35} ${-w * .7} ${-len * .8} 0 ${-len} C${w * .7} ${-len * .8} ${w} ${-len * .35} 0 0Z" fill="${c}"/>`;
const leaf = (x, y, r, s, c, v) => `<g transform="translate(${x} ${y}) rotate(${r}) scale(${s})"><path d="M0 0 C-34 -44 -24 -118 0 -150 C24 -118 34 -44 0 0Z" fill="${c}"/><path d="M0 -6 V-120" stroke="${v}" stroke-width="3" opacity=".6"/></g>`;
const mirrorAt = (w, s) => `<g transform="translate(${w} 0) scale(-1 1)">${s}</g>`;

// ------------------------------------------------------------------------------------------------ Halloween
const pumpBase = `<ellipse cy="9.5" rx="10" ry="1.6" fill="#000" opacity=".25"/><ellipse cx="-8" rx="4.6" ry="8.4" fill="#c25a10"/><ellipse cx="8" rx="4.6" ry="8.4" fill="#c25a10"/><ellipse cx="-4.2" rx="5.4" ry="9" fill="#df771a"/><ellipse cx="4.2" rx="5.4" ry="9" fill="#df771a"/><ellipse rx="5.2" ry="9.4" fill="#f58f2a"/><path d="M-4.2 -8.5 Q-6.2 0 -4.2 8.7 M4.2 -8.5 Q6.2 0 4.2 8.7 M-8 -7 Q-10.4 0 -8 7.2 M8 -7 Q10.4 0 8 7.2" stroke="#b04d0b" stroke-width=".6" fill="none"/><path d="M-1.8 -8.6 Q-2.4 -12.6 1.4 -13.8 L3.2 -12.2 Q1.2 -11.4 1.8 -8.6Z" fill="#6b4a1e"/><path d="M-.6 -9.5 Q-.8 -12 .6 -12.8" stroke="#9a6d30" stroke-width=".6" fill="none"/><path d="M3 -11.6 Q9 -15 11 -10 Q8.5 -12 6.5 -10.5" stroke="#4f9a3c" stroke-width="1" fill="none"/><path d="M5.6 -12.4 Q9.6 -16 12 -13 Q8.8 -11.2 5.6 -12.4Z" fill="#5aa845"/><path d="M-5.6 -3 L-3.4 -6.4 L-1.2 -3Z M1.2 -3 L3.4 -6.4 L5.6 -3Z" fill="#ffe27a"/><path d="M-5.6 2 L-3.6 4.4 L-1.8 2.4 L0 5 L1.8 2.4 L3.6 4.4 L5.6 2 L4.8 6.6 Q0 9 -4.8 6.6Z" fill="#ffe27a"/><ellipse cx="-7" cy="-3" rx="1" ry="3" fill="#fbb868" opacity=".75"/>`;
const pump = pumpBase + '<path d="M3 -9.4 C12 -9 13 6 8 8.4 Q4 9.4 3 9.4Z" fill="#000" opacity=".16"/>';
const bat = '<path d="M0 -3 L1.5 -6 L3 -3 Q6 -6 11 -4 Q9 -2 9.5 1 Q7 -1 5.5 1.5 Q4 -.5 2.5 2 Q1 4 0 5 Q-1 4 -2.5 2 Q-4 -.5 -5.5 1.5 Q-7 -1 -9.5 1 Q-9 -2 -11 -4 Q-6 -6 -3 -3 L-1.5 -6Z" fill="#8f73d8"/><path d="M0 -3 L1.5 -6 L3 -3 Q6 -6 11 -4 Q9 -2 9.5 1 Q7 -1 5.5 1.5 Q4 -.5 2.5 2 Q1 4 0 5Z" fill="#000" opacity=".18"/><ellipse cy="-.5" rx="2" ry="3" fill="#5c45a3"/><circle cx="-.9" cy="-2" r=".6" fill="#ff6b6b"/><circle cx=".9" cy="-2" r=".6" fill="#ff6b6b"/>';
const ghost = '<path d="M-7 8 L-7 -2 Q-7 -9 0 -9 Q7 -9 7 -2 L7 8 L4.7 5.5 L2.3 8 L0 5.5 L-2.3 8 L-4.7 5.5Z" fill="#f2f2f7"/><path d="M2 -8.8 Q7 -9 7 -2 L7 8 L4.7 5.5 L2.3 8 L2 5Z" fill="#000" opacity=".12"/><ellipse cx="-2.6" cy="-2.5" rx="1.3" ry="1.9" fill="#2a2a38"/><ellipse cx="2.6" cy="-2.5" rx="1.3" ry="1.9" fill="#2a2a38"/><ellipse cy="2" rx="1.4" ry="2" fill="#2a2a38"/><ellipse cx="-5" cy="1" rx="1.3" ry=".8" fill="#f5b5c8"/><ellipse cx="5" cy="1" rx="1.3" ry=".8" fill="#f5b5c8"/>';
const candy = '<path d="M0 -8 L1.83 -3 H-1.83Z" fill="#f6f1e7"/><path d="M-1.83 -3 H1.83 L3.67 2 H-3.67Z" fill="#f08a24"/><path d="M-3.67 2 H3.67 L5.5 7 Q0 8.6 -5.5 7Z" fill="#f2c230"/><path d="M1 -3 L3.6 2 L5.4 7 Q3 8 1.5 8Z" fill="#000" opacity=".12"/><path d="M-1 -2 L-2.4 5" stroke="#fff" stroke-width=".6" opacity=".4"/>';
const spiderBody = '<circle r="3.2" fill="#2f2f3d"/><circle cy="-3.3" r="2" fill="#2f2f3d"/><path d="M-3 -1 L-8 -4 L-10 1 M-3 1 L-8 3 L-9 8 M3 -1 L8 -4 L10 1 M3 1 L8 3 L9 8" stroke="#2f2f3d" stroke-width="1" fill="none"/><circle cx="-.8" cy="-3.6" r=".5" fill="#ff6b6b"/><circle cx=".8" cy="-3.6" r=".5" fill="#ff6b6b"/>';

// The crescent moon: the part of circle 1 that circle 2 does not cover (a real shape, so it needs no background colour).
const crescent = (() => {
  const c1 = [0, 0], r1 = 5.5, c2 = [2.4, -1.7], r2 = 4.8;
  const dx = c2[0] - c1[0], dy = c2[1] - c1[1], d = Math.hypot(dx, dy), ux = dx / d, uy = dy / d;
  const a = (r1 * r1 - r2 * r2 + d * d) / (2 * d), h = Math.sqrt(r1 * r1 - a * a);
  const P1 = [a * ux - h * uy, a * uy + h * ux], P2 = [a * ux + h * uy, a * uy - h * ux];
  const ang = (p, c) => Math.atan2(p[1] - c[1], p[0] - c[0]);
  const inc = (from, via, to, c) => { const t = x => ((x % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI); return t(ang(via, c) - ang(from, c)) < t(ang(to, c) - ang(from, c)); };
  const far = [c1[0] - ux * r1, c1[1] - uy * r1], near = [c2[0] - ux * r2, c2[1] - uy * r2];
  const sw1 = inc(P1, far, P2, c1) ? 1 : 0, sw2 = inc(P2, near, P1, c2) ? 1 : 0;
  return `<path d="M${f1(P1[0])} ${f1(P1[1])} A${r1} ${r1} 0 1 ${sw1} ${f1(P2[0])} ${f1(P2[1])} A${r2} ${r2} 0 0 ${sw2} ${f1(P1[0])} ${f1(P1[1])}Z" fill="#f7e08a"/>`;
})();
const hHero = `<circle cx="-13" cy="-14" r="9" fill="#f7e08a" opacity=".12"/><g transform="translate(-13 -14)">${crescent}</g><g transform="translate(6 -21)">${stars('#fff', 1.5, .6)}</g><g transform="translate(-4 -23)">${stars('#fff', 1, .4)}</g><g transform="translate(18 -17)">${stars('#fff', 1.2, .5)}</g><g transform="translate(12 -12) scale(.7)">${bat}</g><g transform="translate(-1 -15) scale(.5)">${bat}</g><ellipse cx="2" cy="15" rx="24" ry="3" fill="#000" opacity=".3"/><circle cx="3" cy="4" r="17" fill="#ffd35a" opacity=".12"/><g transform="translate(-14 11) scale(.6)">${pump}</g><g transform="translate(3 4) scale(1.3)">${pump}</g><g transform="translate(17 11) rotate(10)">${candy}</g>`;

// The spider's web for the corner of the picture area (100 x 100, the corner is at the top right).
const web = (() => {
  const cx = 100, ang = [90, 105, 120, 135, 150, 165, 180].map(d => d * Math.PI / 180);
  const P = (r, a) => [cx + r * Math.cos(a), r * Math.sin(a)];
  let s = '<g stroke="#d6d6e0" fill="none" stroke-linecap="round" opacity=".85">';
  ang.forEach(a => { const [x, y] = P(98, a); s += `<path d="M${cx} 0 L${f1(x)} ${f1(y)}" stroke-width=".9"/>`; });
  [12, 24, 38, 54, 72, 92].forEach(r => { for (let i = 0; i < ang.length - 1; i++) { const [x1, y1] = P(r, ang[i]), [x2, y2] = P(r, ang[i + 1]), [mx, my] = P(r * .9, (ang[i] + ang[i + 1]) / 2); s += `<path d="M${f1(x1)} ${f1(y1)} Q${f1(mx)} ${f1(my)} ${f1(x2)} ${f1(y2)}" stroke-width=".6"/>`; } });
  s += '</g>';
  [[38, 2], [54, 4], [72, 1], [24, 3], [92, 5]].forEach(([r, i]) => { const [x, y] = P(r * .97, ang[i]); s += `<circle cx="${f1(x)}" cy="${f1(y)}" r="1.1" fill="#fff" opacity=".85"/>`; });
  const [sx, sy] = P(46, ang[3]);
  return s + `<g transform="translate(${f1(sx)} ${f1(sy)}) scale(.9)">${spiderBody}</g>`;
})();

// ------------------------------------------------------------------------------------------------ Día de los Muertos
// The papel picado: one even-odd path, so the cut-outs are real holes.
const flag = c => {
  let top = 'M-9 -10 ';
  for (const k of [-7.5, -4.5, -1.5, 1.5, 4.5, 7.5]) top += `H${k - 1.2} L${k} -8.2 L${k + 1.2} -10 `;
  top += 'H9 V7 L7.2 10 L5.4 7 L3.6 10 L1.8 7 L0 10 L-1.8 7 L-3.6 10 L-5.4 7 L-7.2 10 L-9 7Z ';
  let holes = '';
  for (let i = 0; i < 6; i++) { const a = i * Math.PI / 3; holes += circ(f1(Math.cos(a) * 3), f1(Math.sin(a) * 3 - 1), 1.2) + ' '; }
  holes += circ(0, -1, 1.2) + ' M-6.4 -1 l1.2 2 l-1.2 2 l-1.2 -2z M6.4 -1 l1.2 2 l-1.2 2 l-1.2 -2z ' + circ(-5, 5.2, .9) + ' ' + circ(0, 6, .9) + ' ' + circ(5, 5.2, .9);
  return `<path d="${top}${holes}" fill="${c}" fill-rule="evenodd"/><path d="M0 -10 H9 V7 L7.2 10 L5.4 7 L3.6 10 L1.8 7 L0 10Z" fill="#000" opacity=".1"/><rect x="-9" y="-10" width="18" height="2.4" fill="#000" opacity=".12"/>`;
};
const mari = ringE(16, 6.6, 2.2, 4.2, '#e8800f') + ringE(12, 4.6, 1.9, 3.4, '#f9a825') + ringE(8, 2.8, 1.5, 2.4, '#ffc94a') + '<circle r="1.6" fill="#b3500c"/>';
const candleSmall = '<circle cy="-8" r="8" fill="#ffd35a" opacity=".18"/><ellipse cy="10" rx="6" ry="1.8" fill="#c99a1c"/><rect x="-3" y="-2" width="6" height="12" fill="#f6f1e7"/><rect x="-3" y="-2" width="2" height="12" fill="#fff" opacity=".6"/><rect x="1" y="-2" width="2" height="12" fill="#000" opacity=".1"/><ellipse cy="-2" rx="3" ry="1" fill="#e9e1cf"/><path d="M0 -3 Q-3 -7 0 -12 Q3 -7 0 -3Z" fill="#ffd35a"/><path d="M0 -3.5 Q-1.3 -6 0 -8.5 Q1.3 -6 0 -3.5Z" fill="#f08a1a"/>';

// The sugar skull with a candle on its head, after the reference picture. Drawn in a 870-wide frame: the face is
// x 40..830, the eyes at y 535, the chin at y 1080.
const PU = '#4b0b50', CR = '#c0224e', TL = '#3a8aa8', BLU = '#3f86b0';
const m870 = s => mirrorAt(870, s);
const SQ = 'M-100 0 C-100 -62 -62 -100 0 -100 C62 -100 100 -62 100 0 C100 62 62 100 0 100 C-62 100 -100 62 -100 0Z';
const eyeYellow = (() => {
  let s = rot(28, i => `<g transform="rotate(6.4)"><rect x="-8" y="-140" width="16" height="44" rx="8" fill="#a02850"/></g>`);
  s += `<circle r="122" fill="#e9a21a"/>` + rot(28, i => `<rect x="-12" y="-150" width="24" height="56" rx="12" fill="${i % 2 ? '#f6b81c' : '#f8d02c'}"/>`);
  return s + `<circle r="112" fill="#e9a21a"/><path d="${SQ}" fill="${PU}"/><ellipse cx="-34" cy="-40" rx="24" ry="14" fill="#fff" opacity=".06" transform="rotate(-35)"/>`;
})();
const eyeBlue = (() => {
  let s = rot(22, i => `<circle cy="-138" r="12" fill="#a02850"/>`);
  s += rot(22, i => `<g transform="rotate(8.2)"><circle cy="-128" r="22" fill="#2f6f9c"/></g>`) + rot(22, i => `<circle cy="-126" r="20" fill="${BLU}"/>`);
  return s + `<circle r="128" fill="${BLU}"/><circle r="114" fill="none" stroke="#6fb0d8" stroke-width="8" opacity=".6"/><path d="${SQ}" fill="${PU}"/>`;
})();
const foreL = `<path d="M60 300 C50 250 120 225 150 262 C168 296 125 330 100 306 C90 292 104 276 118 286" fill="none" stroke="${CR}" stroke-width="13" stroke-linecap="round"/><path d="M132 252 C190 212 262 240 302 288" fill="none" stroke="${CR}" stroke-width="13" stroke-linecap="round"/><path d="M176 250 C160 200 196 176 212 206 C216 228 196 244 190 256" fill="none" stroke="#5b1a5e" stroke-width="15" stroke-linecap="round"/><path d="M96 180 C108 150 142 146 156 168" fill="none" stroke="${BLU}" stroke-width="9" stroke-linecap="round"/><path d="M104 196 C120 180 140 182 146 194" fill="none" stroke="#2b4f7a" stroke-width="7" stroke-linecap="round"/><path d="M236 196 C240 150 292 140 302 176 C302 196 278 202 272 188" fill="none" stroke="#8fb6d2" stroke-width="11" stroke-linecap="round"/><circle cx="250" cy="196" r="7" fill="#2b4f7a"/><path d="M172 365 C188 322 246 306 272 338" fill="none" stroke="#8fb6d2" stroke-width="11" stroke-linecap="round"/><path d="M200 345 C215 330 236 332 244 346" fill="none" stroke="#e8a090" stroke-width="8" stroke-linecap="round"/><path d="M302 352 C318 306 362 298 376 326 C360 332 344 348 324 384" fill="none" stroke="#e0607e" stroke-width="9" stroke-linecap="round"/><path d="M316 366 C330 358 350 360 358 372" fill="none" stroke="#3f86b0" stroke-width="7" stroke-linecap="round"/><path d="M48 360 C52 410 70 448 88 478 C112 446 122 404 104 364Z" fill="#e8402e"/><path d="M70 392 C76 420 84 440 90 456" fill="none" stroke="#5b1a5e" stroke-width="6" stroke-linecap="round"/><path d="M108 392 C140 372 168 386 150 420" fill="none" stroke="${CR}" stroke-width="8" stroke-linecap="round"/><circle cx="372" cy="110" r="7" fill="${CR}"/><circle cx="406" cy="96" r="6" fill="${CR}"/>`;
const foreR = `<path d="M60 300 C50 250 120 225 150 262 C168 296 125 330 100 306 C90 292 104 276 118 286" fill="none" stroke="${CR}" stroke-width="13" stroke-linecap="round"/><path d="M132 252 C190 212 262 240 302 288" fill="none" stroke="${CR}" stroke-width="13" stroke-linecap="round"/><path d="M176 250 C160 200 196 176 212 206 C216 228 196 244 190 256" fill="none" stroke="#5b1a5e" stroke-width="15" stroke-linecap="round"/><path d="M96 178 C108 148 142 144 156 166" fill="none" stroke="#3aa0a8" stroke-width="9" stroke-linecap="round"/><path d="M236 196 C240 150 292 140 302 176 C302 196 278 202 272 188" fill="none" stroke="#f6d23a" stroke-width="9" stroke-linecap="round"/><path d="M48 360 C52 410 70 448 88 478 C112 446 122 404 104 364Z" fill="#e8402e"/><path d="M108 392 C140 372 168 386 150 420" fill="none" stroke="${CR}" stroke-width="8" stroke-linecap="round"/><circle cx="372" cy="110" r="7" fill="${CR}"/>`;
const cheekL = `<path d="M104 640 C100 735 192 812 274 788 C252 742 192 702 160 648 C142 614 112 612 104 640Z" fill="#f9a030"/><path d="M128 716 C170 790 240 806 276 790 C258 822 188 822 140 764Z" fill="#fbd24a"/><circle cx="176" cy="698" r="30" fill="#fff2e0"/><path d="M148 690 C140 654 194 640 204 678 C210 708 172 722 160 700 C154 686 172 678 180 688" fill="none" stroke="${CR}" stroke-width="11" stroke-linecap="round"/><path d="M258 756 C254 714 322 696 352 722 C364 744 346 774 320 778 C296 781 284 762 298 752 C306 746 320 752 316 760" fill="#fff" stroke="${CR}" stroke-width="10" stroke-linecap="round"/><path d="M300 728 C318 720 338 728 340 746" fill="none" stroke="${PU}" stroke-width="8" stroke-linecap="round"/>`;
const teeth = [[262, 306, 842, 894], [310, 362, 832, 902], [366, 430, 826, 912], [440, 504, 826, 912], [508, 560, 832, 902], [564, 608, 842, 894]].map(([a, b, t, bt]) => `<path d="M${a} ${bt} V${t + 18} Q${a} ${t} ${(a + b) / 2} ${t} Q${b} ${t} ${b} ${t + 18} V${bt}Z" fill="${PU}"/>`).join('');
const dotsRow = [282, 342, 402, 465, 527, 587].map(x => `<circle cx="${x}" cy="812" r="8" fill="${TL}"/>`).join('') + `<circle cx="636" cy="822" r="8" fill="${TL}"/><circle cx="632" cy="892" r="5" fill="${TL}"/>`;
const FACE_TAIL = 'C45 500 60 540 75 590 C100 650 150 720 175 790 C185 840 190 900 205 960 C230 1020 300 1070 435 1080 C570 1070 640 1020 665 960 C680 900 685 840 695 790 C720 720 770 650 795 590 C810 540 825 500 830 440';
const faceFlat = `M40 -100 L40 440 ${FACE_TAIL} L830 -100Z`;                         // the candle hides the top
const faceDome = `M40 300 L40 440 ${FACE_TAIL} L830 300 C830 110 680 -20 435 -20 C190 -20 40 110 40 300Z`; // a whole head
const ribbons = (() => {
  const r = `<path d="M100 575 C10 640 -10 740 60 810 C112 862 142 905 160 958 C168 978 184 988 200 984 L240 930 L240 760 L130 600Z" fill="#5a9fd0"/><path d="M100 640 C60 700 80 770 140 815 C160 832 174 860 182 915 L240 920 L240 800 L150 650Z" fill="#3b73a3"/><path d="M-6 700 C-22 760 -4 800 34 826 C10 790 4 750 22 712Z" fill="#2d7a80"/>`;
  return r + m870(r);
})();
const skullFace = dome => `${ribbons}<path d="${dome ? faceDome : faceFlat}" fill="#fcf0e2"/>
<path d="M40 200 L40 440 C45 500 60 540 75 590 C90 616 104 640 112 660 C70 560 62 400 78 200Z" fill="#f3d3b3"/>${m870('<path d="M40 200 L40 440 C45 500 60 540 75 590 C90 616 104 640 112 660 C70 560 62 400 78 200Z" fill="#f6e0c8"/>')}
<path d="M285 276 C340 226 420 202 470 214 C520 202 600 226 660 276 C600 254 520 240 470 250 C420 240 340 254 285 276Z" fill="#f4702a"/><path d="M330 262 C380 236 430 224 470 232 C520 224 570 238 612 262 C560 250 520 246 470 252 C420 246 380 250 330 262Z" fill="#ffb878" opacity=".85"/>
${foreL}${m870(foreR)}
<g transform="translate(430 195) scale(1.18) translate(-430 -195)"><path d="M372 205 C372 245 420 266 462 242 C500 218 495 152 440 142 C400 136 372 162 388 190 C400 210 440 207 445 184 C448 170 430 167 424 180" fill="none" stroke="#4a1450" stroke-width="27" stroke-linecap="round"/></g>
<path d="M345 192 C320 132 360 76 425 70 C480 66 506 112 496 162" fill="none" stroke="${CR}" stroke-width="13" stroke-linecap="round"/><path d="M512 128 C524 100 560 96 572 124" fill="none" stroke="${CR}" stroke-width="9" stroke-linecap="round"/>
<path d="M480 392 C500 342 545 318 578 308 C578 344 542 388 480 392Z" fill="#f58a2a"/><path d="M500 372 C520 352 545 338 566 326" stroke="#ffd27a" stroke-width="7" fill="none" stroke-linecap="round"/>
<path d="M592 360 C586 324 640 306 666 326 C682 342 666 368 640 360" fill="none" stroke="#f6d23a" stroke-width="16" stroke-linecap="round"/><circle cx="650" cy="346" r="14" fill="#3aa08a"/><path d="M592 306 C622 284 682 290 704 332" fill="none" stroke="${BLU}" stroke-width="7" stroke-linecap="round"/>
<path d="M404 296 C404 268 466 268 466 296 C466 336 404 336 404 296Z" fill="${TL}"/><path d="M406 292 C410 270 462 270 464 292Z" fill="#2b4f7a"/>
<circle cx="435" cy="372" r="11" fill="${TL}"/><circle cx="435" cy="420" r="11" fill="${TL}"/><circle cx="435" cy="468" r="11" fill="#f6d23a"/><circle cx="435" cy="513" r="10" fill="#f6d23a"/><circle cx="435" cy="556" r="9" fill="#f6d23a"/>
<g transform="translate(268 535)">${eyeYellow}</g><g transform="translate(595 535)">${eyeBlue}</g>
<path d="M435 640 C415 640 395 710 380 762 C376 776 394 776 406 768 C420 758 428 748 435 748 C442 748 450 758 464 768 C476 776 494 776 490 762 C475 710 455 640 435 640Z" fill="${PU}"/>
${m870(cheekL)}${cheekL}${dotsRow}
<path d="M228 818 C236 860 262 888 304 896 L566 896 C608 888 634 860 642 818" fill="none" stroke="${PU}" stroke-width="8" stroke-linecap="round"/>${teeth}
<path d="M242 932 C300 926 392 962 428 1048 C352 1044 270 1004 242 932Z" fill="#f58a2a"/><path d="M270 940 C320 944 380 980 410 1030" fill="none" stroke="#fbb040" stroke-width="10" stroke-linecap="round"/><ellipse cx="290" cy="958" rx="12" ry="8" fill="#fbd24a" transform="rotate(30 290 958)"/>
<path d="M450 1010 C470 940 560 920 620 940 C640 960 620 1000 580 1000 C550 1000 545 970 570 968" fill="none" stroke="${CR}" stroke-width="12" stroke-linecap="round"/><path d="M440 1040 C450 990 500 960 540 960" fill="none" stroke="#fbd38a" stroke-width="14" stroke-linecap="round"/>`;
const skullMini = `<g transform="scale(.02) translate(-435 -530)">${skullFace(true)}</g>`;

const dahlia = (x, y, s) => `<g transform="translate(${x} ${y}) scale(${s})">${rot(18, i => `<g transform="rotate(10)">${petal(125, 26, '#8e1a5b')}</g>`)}${rot(14, i => petal(95, 24, '#a8236a'))}${rot(12, i => `<g transform="rotate(12)">${petal(66, 20, '#c93b86')}</g>`)}<circle r="30" fill="#e0a020"/><circle r="24" fill="#f6c52a"/><circle r="9" fill="#e0a020"/></g>`;
const marigold = (x, y, s) => `<g transform="translate(${x} ${y}) scale(${s})">${rot(22, i => petal(100, 30, '#d83a2a'))}${rot(18, i => `<g transform="rotate(8)">${petal(82, 28, '#f26a2a')}</g>`)}${rot(14, i => petal(62, 24, '#f99a2b'))}${rot(10, i => `<g transform="rotate(9)">${petal(42, 20, '#fbbf3a')}</g>`)}<circle r="16" fill="#e8402e"/><circle r="7" fill="#f6c52a"/></g>`;
const bigYellow = (x, y, s, r) => `<g transform="translate(${x} ${y}) rotate(${r}) scale(${s})">${rot(12, i => petal(110, 34, '#f9a825'))}${rot(12, i => `<g transform="rotate(15)">${petal(82, 30, '#fbc23a')}</g>`)}${rot(8, i => petal(52, 22, '#fbd24a'))}<circle r="14" fill="#e8402e"/><circle r="6" fill="#8e1a5b"/></g>`;

const CT = -170; // the top of the candle
const mHero = `${leaf(435, 1540, 0, 1.1, '#4a7a98', '#fff')}${leaf(280, 1490, -35, 1.1, '#2a5470', '#fff')}${leaf(590, 1490, 35, 1.1, '#2a5470', '#fff')}${leaf(-40, 1330, -60, 1.1, '#2a5470', '#fff')}${leaf(910, 1330, 60, 1.1, '#2a5470', '#fff')}${leaf(20, 1180, -100, .9, '#6a9ab8', '#fff')}${leaf(850, 1180, 100, .9, '#6a9ab8', '#fff')}
${skullFace(false)}
<path d="M30 ${CT} V215 C80 160 160 120 260 98 C330 80 380 66 435 62 C490 66 540 80 610 98 C710 120 790 160 840 215 V${CT}Z" fill="#ee5528"/><path d="M435 ${CT} V62 C490 66 540 80 610 98 C710 120 790 160 840 215 V${CT}Z" fill="#f99a2b"/><path d="M30 ${CT} V-60 H840 V${CT}Z" fill="#ffb347" opacity=".25"/>
<ellipse cx="435" cy="${CT}" rx="405" ry="40" fill="#ffb03a"/><path d="M30 ${CT} A405 40 0 0 0 435 ${CT + 40} V${CT}Z" fill="#f08a28"/><ellipse cx="435" cy="${CT + 2}" rx="352" ry="26" fill="#fff07a"/><ellipse cx="435" cy="${CT + 6}" rx="260" ry="15" fill="#fff8b0"/>
<path d="M435 ${CT - 14} C402 ${CT - 70} 405 ${CT - 130} 435 ${CT - 180} C465 ${CT - 130} 468 ${CT - 70} 435 ${CT - 14}Z" fill="#fff5b8"/><path d="M435 ${CT - 24} C416 ${CT - 62} 419 ${CT - 100} 435 ${CT - 130} C451 ${CT - 100} 454 ${CT - 62} 435 ${CT - 24}Z" fill="#fffbe0"/><path d="M435 ${CT} C426 ${CT - 14} 428 ${CT - 28} 435 ${CT - 40} C442 ${CT - 28} 444 ${CT - 14} 435 ${CT}Z" fill="#f0602a"/>
<g transform="translate(195 -45)">${rot(10, i => `<g transform="rotate(18)">${petal(105, 34, i % 2 ? '#e8402e' : '#f0553a')}</g>`)}${rot(8, i => petal(70, 26, '#f77a3a'))}<circle r="30" fill="#ffc83a"/><circle r="14" fill="#f6a020"/></g>
<g transform="translate(662 -45)">${rot(12, i => `<g transform="rotate(15)">${petal(100, 30, i % 2 ? '#f26a2a' : '#e8402e')}</g>`)}${rot(10, i => petal(64, 24, '#f99a2b'))}<circle r="26" fill="#ffd23a"/><circle r="10" fill="#f6a020"/></g>
<path d="M150 70 C158 120 180 158 196 178 C216 148 226 106 222 66Z" fill="#e8402e"/><path d="M646 78 C654 126 676 160 692 180 C712 150 720 110 716 74Z" fill="#e8402e"/>
<path d="M44 205 C36 130 70 76 154 54 C144 116 112 176 44 205Z" fill="#25506e"/><path d="M60 180 C80 130 110 96 140 74" stroke="#4a7a98" stroke-width="5" fill="none"/><path d="M826 205 C834 130 800 76 716 54 C726 116 758 176 826 205Z" fill="#2f5f80"/><path d="M810 180 C790 130 760 96 730 74" stroke="#5a8aa8" stroke-width="5" fill="none"/>
${bigYellow(-20, 1100, 1.45, -20)}${bigYellow(890, 1100, 1.45, 20)}${dahlia(150, 1240, 1.7)}${dahlia(720, 1240, 1.7)}${marigold(435, 1150, 2.3)}`;

// ------------------------------------------------------------------------------------------------ Navidad
const tierPath = (top, bot, w, half) => { const n = 4, seg = 2 * w / n; let d; if (!half) { d = `M0 ${top} L${-w} ${bot}`; for (let k = 0; k < n; k++) { const x0 = -w + k * seg; d += ` Q${x0 + seg / 2} ${bot - 4} ${x0 + seg} ${bot}`; } return d + 'Z'; } d = `M0 ${top} L${w} ${bot}`; for (let k = n - 1; k >= n / 2; k--) { const x0 = -w + k * seg; d += ` Q${x0 + seg / 2} ${bot - 4} ${x0} ${bot}`; } return d + 'Z'; };
const tree = (() => {
  let s = '<rect x="-2.4" y="14" width="4.8" height="4.6" fill="#6b4a1e"/>';
  [[-12, -2, 8, '#37a352'], [-7, 4, 11.5, '#2f8f48'], [-1, 10, 15, '#2a8040'], [5, 16.5, 19, '#256f38']].forEach(([t, b, w, c]) => { s += `<path d="${tierPath(t, b, w, 0)}" fill="${c}"/><path d="${tierPath(t, b, w, 1)}" fill="#000" opacity=".2"/><ellipse cx="${-w + 1.6}" cy="${b - 1.4}" rx="2.2" ry="1" fill="#fff" opacity=".9"/><ellipse cx="${w - 1.6}" cy="${b - 1.4}" rx="2.2" ry="1" fill="#fff" opacity=".9"/>`; });
  s += '<g stroke="#f2c230" stroke-width=".9" fill="none"><path d="M-6 -3.5 Q0 0 6 -3.5"/><path d="M-9.5 2 Q0 7 9.5 2"/><path d="M-13 8 Q0 14 13 8"/></g>';
  [[-3, -1.4, '#d93a3a'], [3.5, 0.2, '#f2c230'], [-6, 5.6, '#4a8ee0'], [1, 6.2, '#d93a3a'], [7, 5, '#f2c230'], [-10, 11.8, '#f2c230'], [-3, 12, '#d93a3a'], [5, 12.4, '#4a8ee0'], [11, 10.6, '#d93a3a']].forEach(([x, y, c]) => { s += `<circle cx="${x}" cy="${y}" r="1.6" fill="${c}"/><circle cx="${x - .5}" cy="${y - .5}" r=".5" fill="#fff" opacity=".7"/>`; });
  [[0, -8, '#fff3a0'], [-5, 0.5, '#8ad0ff'], [5.5, 2.2, '#ff9aa8'], [-8, 8.5, '#fff3a0'], [8, 8, '#8ad0ff'], [0, 9, '#ff9aa8'], [-14, 14.5, '#fff3a0'], [14, 14, '#ff9aa8']].forEach(([x, y, c]) => { s += `<circle cx="${x}" cy="${y}" r="1.7" fill="${c}" opacity=".25"/><circle cx="${x}" cy="${y}" r=".7" fill="${c}"/>`; });
  return s + `<circle cy="-15.5" r="6" fill="#f7e08a" opacity=".2"/><g transform="translate(0 -15.5)">${stars('#f2c230', 5.2, 2.2)}<g transform="rotate(36)">${stars('#fff3a0', 2.4, 1)}</g></g>`;
})();
const bulb = c => `<circle cy="-1" r="9" fill="${c}" opacity=".16"/><rect x="-2.5" y="-10" width="5" height="3.5" rx="1" fill="#9a8a5a"/><ellipse cy="-1" rx="5.5" ry="7" fill="${c}"/><ellipse cx="-2" cy="-3" rx="1.2" ry="2.4" fill="#fff" opacity=".45"/>`;
const ball = c => `<rect x="-2" y="-10" width="4" height="3" fill="#d8b04a"/><circle r="7" fill="${c}"/><path d="M-6.6 -2 Q0 1 6.6 -2 M-6.4 2.5 Q0 5.5 6.4 2.5" stroke="#f5d878" stroke-width="1.4" fill="none"/><ellipse cx="-2.5" cy="-3.5" rx="1.3" ry="2" fill="#fff" opacity=".4"/>`;
const cane = '<path d="M3 9 V-4 A4 4 0 1 0 -5 -4" stroke="#f4f4f4" stroke-width="3.4" fill="none" stroke-linecap="round"/><path d="M3 9 V-4 A4 4 0 1 0 -5 -4" stroke="#d93a3a" stroke-width="3.4" fill="none" stroke-dasharray="2 2.2"/>';
const bell = '<path d="M-6 5 Q-5 -1 -4.5 -4 Q-4 -8 0 -8 Q4 -8 4.5 -4 Q5 -1 6 5Z" fill="#e8b830"/><rect x="-7" y="4" width="14" height="2" rx="1" fill="#c99a1c"/><circle cy="8" r="1.8" fill="#c99a1c"/><path d="M0 -8 L-3.5 -10.5 L-3.5 -5.5Z M0 -8 L3.5 -10.5 L3.5 -5.5Z" fill="#d93a3a"/><ellipse cx="-2" cy="-3" rx=".9" ry="2.4" fill="#fff" opacity=".4"/>';
const holly = '<ellipse cx="-4.5" cy="-1" rx="6" ry="2.7" transform="rotate(-28 -4.5 -1)" fill="#3fae5a"/><ellipse cx="4.5" cy="-1" rx="6" ry="2.7" transform="rotate(28 4.5 -1)" fill="#2f8f48"/><path d="M-9 -4 L-1 0 M9 -4 L1 0" stroke="#1f6a35" stroke-width=".7"/><circle cx="-2" cy="2.5" r="2.2" fill="#d93a3a"/><circle cx="2" cy="3" r="2.2" fill="#d93a3a"/><circle cx="0" cy="0" r="2.2" fill="#e24b4b"/><circle cx="-.7" cy="-.7" r=".6" fill="#fff" opacity=".7"/>';
const gift = (a, b) => `<rect x="-7" y="-4" width="14" height="11" fill="${a}"/><rect x="0" y="-4" width="7" height="11" fill="#000" opacity=".14"/><rect x="-1.3" y="-4" width="2.6" height="11" fill="${b}"/><rect x="-8" y="-7.5" width="16" height="4" fill="${a}"/><rect x="0" y="-7.5" width="8" height="4" fill="#000" opacity=".14"/><rect x="-1.3" y="-7.5" width="2.6" height="4" fill="${b}"/><path d="M0 -7.5 Q-6 -13 -4 -8 Z M0 -7.5 Q6 -13 4 -8 Z" fill="${b}"/><rect x="-7" y="5" width="14" height="2" fill="#000" opacity=".15"/>`;
const xmasHero = `${tree}<g transform="translate(22 10) scale(.71)">${gift('#d93a3a', '#f2c230')}</g>`;

// ------------------------------------------------------------------------------------------------ Año Nuevo
const star2 = (c, c2) => stars(c, 10, 4) + `<g transform="rotate(36)">${stars(c2, 5, 2.1)}</g>`;
const balloon = c => `<ellipse cy="-2" rx="6" ry="7.5" fill="${c}"/><path d="M2 -9 Q6 -2 0 5.4 Q6.4 3 6 -2Z" fill="#000" opacity=".12"/><path d="M-1.2 5.8 L1.2 5.8 L0 8Z" fill="${c}"/><path d="M0 8 Q-3 11 0 14" stroke="#aaa" fill="none" stroke-width=".7"/><ellipse cx="-2.3" cy="-5" rx="1.3" ry="2.3" fill="#fff" opacity=".4"/>`;
const burst = (() => { let s = ''; for (let i = 0; i < 10; i++) { const a = i * Math.PI / 5, c = ['#f2c230', '#e8618c', '#4a8ee0'][i % 3]; s += `<path d="M${f1(Math.cos(a) * 3)} ${f1(Math.sin(a) * 3)} L${f1(Math.cos(a) * 9)} ${f1(Math.sin(a) * 9)}" stroke="${c}" stroke-width="1.3" stroke-linecap="round"/><circle cx="${f1(Math.cos(a) * 11)}" cy="${f1(Math.sin(a) * 11)}" r="1" fill="${c}"/>`; } return s; })();
const confetti = piece(-7, -5, 4, 2.2, '#e8618c', 25) + piece(5, -7, 4, 2.2, '#4a8ee0', -30) + piece(-2, 2, 4, 2.2, '#f2c230', 50) + piece(7, 3, 3.4, 2.2, '#3fae5a', 10) + piece(-8, 6, 3.4, 2.2, '#8e63d6', -20) + '<circle cy="-8" r="1.4" fill="#f2c230"/><circle cx="-5" cy="0" r="1.2" fill="#4a8ee0"/><circle cx="8" cy="-1" r="1.3" fill="#e8618c"/><circle cx="3" cy="8" r="1.2" fill="#3fae5a"/><path d="M-4 -2 q2 -3 4 0 t4 0" stroke="#e8618c" stroke-width="1.1" fill="none"/><path d="M-6 8 q2 -3 4 0 t4 0" stroke="#4a8ee0" stroke-width="1.1" fill="none"/><g transform="translate(6 7)">' + stars('#f2c230', 2.8, 1.2) + '</g><g transform="translate(-9 -1)">' + stars('#f7e08a', 2.4, 1) + '</g>';
const popper = '<path d="M-9 9 L-1.5 -4.5 L4.5 1.5Z" fill="#f2c230"/><path d="M-1.5 -4.5 L4.5 1.5 L-9 9Z" fill="#000" opacity=".1"/><path d="M-5.2 2.4 L1.8 -.3 L2.6 .5 L-3.9 4.5Z" fill="#e8618c"/><path d="M-6.4 5.4 L-3.6 8.4 L-8 8.8Z" fill="#4a78b8"/><path d="M-1.5 -4.5 L4.5 1.5" stroke="#fff" stroke-width="1.6"/><path d="M3 -5 q2 -4 5 -2 t3 -5" stroke="#e8618c" stroke-width="1.1" fill="none"/><path d="M6 -2 q4 -2 5 1 t4 -1" stroke="#4a8ee0" stroke-width="1.1" fill="none"/>' + piece(8, -9, 3.6, 2, '#f2c230', 30) + piece(11, -4, 3.6, 2, '#3fae5a', -40) + piece(4, -11, 3.6, 2, '#4a8ee0', 70) + piece(9, 2, 3.2, 2, '#8e63d6', 10) + '<circle cx="12" cy="-8" r="1.1" fill="#e8618c"/><circle cx="6" cy="-7" r="1" fill="#f7e08a"/><circle cx="10" cy="-1" r="1" fill="#f2c230"/>';
const clock = '<circle r="9" fill="#c99a1c"/><circle r="7.6" fill="#f6f1e7"/><path d="M0 -6.6 V-5.2 M6.6 0 H5.2 M0 6.6 V5.2 M-6.6 0 H-5.2" stroke="#2a2a38" stroke-width=".9"/><path d="M0 0 V-5" stroke="#2a2a38" stroke-width="1.2"/><path d="M0 0 V-3.4" stroke="#d93a3a" stroke-width="1.6"/><circle r="1" fill="#2a2a38"/><g transform="translate(0 -11)">' + stars('#f2c230', 3.4, 1.4) + '</g>';
const nHero = (() => {
  let s = `<g transform="translate(-17 -14) scale(.95)">${burst}</g><g transform="translate(17 -10) scale(.75)">${burst}</g><g transform="translate(1 -25) scale(.5)">${burst}</g><g transform="translate(0 5)">`;
  s += '<path d="M-8 11 L-11.5 17 M8 11 L11.5 17" stroke="#a87c14" stroke-width="2" stroke-linecap="round"/><circle cx="-9.6" cy="-11.5" r="3.2" fill="#c99a1c"/><circle cx="9.6" cy="-11.5" r="3.2" fill="#c99a1c"/><circle cx="-9.6" cy="-11.5" r="1.6" fill="#f2c230"/><circle cx="9.6" cy="-11.5" r="1.6" fill="#f2c230"/><path d="M-9.6 -14.5 Q0 -19 9.6 -14.5" stroke="#c99a1c" stroke-width="1.4" fill="none"/><circle cy="-17" r="1.4" fill="#f2c230"/><circle r="13" fill="#c99a1c"/><circle r="11.8" fill="#f2c230"/><circle r="10.4" fill="#f6f1e7"/><path d="M0 -10.4 A10.4 10.4 0 0 1 0 10.4 Q4 0 0 -10.4Z" fill="#000" opacity=".06"/><path d="M5 -8 A10.4 10.4 0 0 1 11.8 0" stroke="#000" stroke-width="1.2" fill="none" opacity=".1"/>';
  for (let i = 0; i < 12; i++) { const a = i * Math.PI / 6, big = i % 3 == 0, r1 = big ? 7.2 : 8.4; s += `<path d="M${(Math.sin(a) * 9.6).toFixed(2)} ${(-Math.cos(a) * 9.6).toFixed(2)} L${(Math.sin(a) * r1).toFixed(2)} ${(-Math.cos(a) * r1).toFixed(2)}" stroke="#2a2a38" stroke-width="${big ? 1.2 : .7}" stroke-linecap="round"/>`; }
  s += '<path d="M0 0 V-5.2" stroke="#2a2a38" stroke-width="1.7" stroke-linecap="round"/><path d="M0 0 V-8" stroke="#2a2a38" stroke-width="1.1" stroke-linecap="round"/><path d="M0 2 L0 -8.8" stroke="#d93a3a" stroke-width=".5"/><circle r="1.3" fill="#d93a3a"/><path d="M-8 -4 A9 9 0 0 1 -3 -8.6" stroke="#fff" stroke-width="1.2" fill="none" opacity=".6" stroke-linecap="round"/></g>';
  s += `<g transform="translate(-20 6)">${stars('#f2c230', 2.6, 1.1)}</g><g transform="translate(21 8)">${stars('#f7e08a', 2.2, .9)}</g><g transform="translate(-18 19)">${stars('#f7e08a', 1.8, .8)}</g>` + piece(18, 19, 3.6, 2, '#e8618c', 30) + piece(-14, 24, 3.6, 2, '#4a8ee0', -40) + piece(10, 25, 3.2, 2, '#3fae5a', 70) + '<circle cx="20" cy="-1" r="1" fill="#e8618c"/><circle cx="-21" cy="14" r="1" fill="#3fae5a"/>';
  return s;
})();

// ------------------------------------------------------------------------------------------------ San Valentín
const heart = (c, l, d) => `<path d="M0 8 C-12 -1 -7 -9 0 -3.5 C7 -9 12 -1 0 8Z" fill="${c}" stroke="${d}" stroke-width=".7"/><path d="M0 8 C-12 -1 -7 -9 0 -3.5 Z" fill="${l}" opacity=".55"/><path d="M0 8 C12 -1 7 -9 0 -3.5 Z" fill="#000" opacity=".12"/><path d="M-6 -4.5 Q-8 -3 -7 -1" stroke="#fff" stroke-width="1.3" fill="none" stroke-linecap="round" opacity=".8"/><circle cx="-3.6" cy="-5.6" r=".8" fill="#fff" opacity=".8"/>`;
const heartRed = heart('#e8496b', '#ff7a96', '#a92c47'), heartPink = heart('#f58aa5', '#ffc0d0', '#c9607c');
const bow = '<path d="M0 -3.6 L-4.4 -7.6 Q-6.6 -4.2 0 -3.6Z M0 -3.6 L4.4 -7.6 Q6.6 -4.2 0 -3.6Z" fill="#f2c230" stroke="#c99a1c" stroke-width=".4"/><path d="M0 -3.6 L-2 0.6 M0 -3.6 L2 0.6" stroke="#c99a1c" stroke-width=".9"/><circle cy="-4" r="1.2" fill="#f7e08a"/>';
const heartBow = heartRed + bow;
const arrow = heartRed + '<path d="M-10 9 L10 -9" stroke="#8a5a3a" stroke-width="1.1"/><path d="M10 -9 L5.6 -9.4 L9.6 -5Z" fill="#8a5a3a"/><path d="M-10 9 L-13 9 L-10 6Z M-8 7 L-11 7 L-8 4Z" fill="#fff"/>';
const env = '<rect x="-10" y="-6.5" width="20" height="13" rx="1.6" fill="#fbe9f0"/><path d="M-10 -6 L0 2 L10 -6" stroke="#e8496b" fill="none" stroke-width="1"/><path d="M0 5 C-5 1 -2 -2 0 0 C2 -2 5 1 0 5Z" fill="#e8496b" transform="translate(0 -1.2) scale(.9)"/>';
const rose = '<path d="M0 6 Q1.5 12 -.6 17" stroke="#2f8f48" stroke-width="1.7" fill="none"/><path d="M.6 11 Q8 9 9 3.5 Q4 4 .6 11Z" fill="#3fae5a"/><path d="M1.6 9 Q5 7.4 7.4 5" stroke="#2a7d40" stroke-width=".5" fill="none"/><path d="M-.5 14 Q-8 12.5 -9 7 Q-3 7.4 -.5 14Z" fill="#2f8f48"/><path d="M-2 11.6 Q-5 10 -7.4 8.4" stroke="#1f6a35" stroke-width=".5" fill="none"/><path d="M1 12 L3 11.4 L1.2 13Z" fill="#2a7d40"/><path d="M-7 -1 Q-8 -8 0 -8 Q8 -8 7 -1 Q6 6 0 7 Q-6 6 -7 -1Z" fill="#c92f50"/><path d="M-5 -1 Q-5.5 -6 0 -6 Q5.5 -6 5 -1 Q4 4.5 0 5 Q-4 4.5 -5 -1Z" fill="#e0435f"/><path d="M-3.2 -1 Q-3.5 -4.5 0 -4.5 Q3.5 -4.5 3.2 -1 Q2.6 3 0 3.2 Q-2.6 3 -3.2 -1Z" fill="#ec6580"/><path d="M-1.8 0 Q-1.5 -2.6 .5 -2.4 Q2 -2 1.6 0 Q1 1.6 -.6 1.2" stroke="#b32548" fill="none" stroke-width=".8"/><path d="M-6.4 -2 Q-4.4 1 -5.2 4.4 M6.4 -2 Q4.4 1 5.2 4.4" stroke="#a82444" stroke-width=".5" fill="none"/><ellipse cx="-3.8" cy="-5" rx="1.1" ry="2" fill="#fff" opacity=".35" transform="rotate(30 -3.8 -5)"/>';
const hp = 'M0 8 C-12 -1 -7 -9 0 -3.5 C7 -9 12 -1 0 8Z';
const vHero = `<circle cy="-1" r="17" fill="#f58aa5" opacity=".12"/><g transform="translate(-15 -14) rotate(-15) scale(.5)">${heartPink}</g><g transform="translate(16 -15) rotate(15) scale(.42)">${heartRed}</g><g transform="translate(18 -3) scale(.3)">${heartPink}</g><g transform="translate(-19 -2) scale(.28)">${heartRed}</g><g transform="translate(0 -2) scale(1.9)">${heartRed}<path d="${hp}" transform="translate(0 .5) scale(.72)" fill="none" stroke="#fff" stroke-width=".55" stroke-dasharray="1 1.1" opacity=".65"/>${bow}</g><g transform="translate(-13 11) rotate(-25) scale(.7)">${rose}</g><g transform="translate(13 12) rotate(25) scale(.6)">${rose}</g><g transform="translate(8 -20)">${stars('#fff', 1.6, .6)}</g><g transform="translate(-6 -21)">${stars('#fff', 1.1, .45)}</g><g transform="translate(20 8)">${stars('#fff', 1.2, .5)}</g>`;

// ------------------------------------------------------------------------------------------------ Noche de Reyes
const crown = '<path d="M-9 6 V-4 L-5 0 L0 -8 L5 0 L9 -4 V6Z" fill="#f2c230"/><rect x="-9" y="3" width="18" height="3" fill="#c99a1c"/><circle cx="-9" cy="-4" r="1.4" fill="#fff"/><circle cy="-8" r="1.4" fill="#fff"/><circle cx="9" cy="-4" r="1.4" fill="#fff"/><circle cx="-4.5" cy="4.5" r="1.1" fill="#4a8ee0"/><circle cy="4.5" r="1.1" fill="#d93a3a"/><circle cx="4.5" cy="4.5" r="1.1" fill="#3fae5a"/><path d="M0 -8 L5 0 L9 -4 V6 H0Z" fill="#000" opacity=".13"/><path d="M-6 -2 L-4.5 1" stroke="#fff" opacity=".5"/>';
const starB = '<circle r="10" fill="#f7e08a" opacity=".16"/><path d="M0 -11 L1.8 -1.8 L11 0 L1.8 1.8 L0 11 L-1.8 1.8 L-11 0 L-1.8 -1.8Z" fill="#f7e08a"/><path d="M0 -6 L1 -1 L6 0 L1 1 L0 6 L-1 1 L-6 0 L-1 -1Z" fill="#fff" transform="rotate(45)"/><circle r="1.6" fill="#fff"/>';
const rHero = (() => {
  let s = '<path d="M0 -26 L-13 6 H13Z" fill="#f7e08a" opacity=".08"/><g transform="translate(0 -27) scale(.62)">' + starB + '</g><g transform="translate(0 -1) scale(1.05)">';
  s += '<path d="M-8.4 -2.4 Q-8.8 -11 0 -11.6 Q8.8 -11 8.4 -2.4Z" fill="#8a2bd0"/><path d="M0 -11.6 Q8.8 -11 8.4 -2.4 H0Z" fill="#000" opacity=".2"/><path d="M-6 -4 Q-6.6 -9 -3 -10" stroke="#c48cf2" stroke-width=".8" fill="none"/><path d="M-8.4 -2.6 Q-8 -12 0 -13.4 Q8 -12 8.4 -2.6" stroke="#f2c230" stroke-width="1.5" fill="none"/><path d="M0 -13.4 V-2" stroke="#f2c230" stroke-width="1.2"/><circle cy="-14.4" r="1.7" fill="#f2c230"/><path d="M0 -16 V-19.6 M-1.7 -18 H1.7" stroke="#f2c230" stroke-width="1.1" stroke-linecap="round"/><path d="M-9.6 -2.4 Q0 .6 9.6 -2.4 V3.6 Q0 6.6 -9.6 3.6Z" fill="#f6f1e7"/><path d="M0 .6 Q4 .3 9.6 -2.4 V3.6 Q4 6 0 6.2Z" fill="#000" opacity=".1"/>';
  [-6.5, -2.2, 2.2, 6.5].forEach(x => { s += `<path d="M${x} 0.6 l.7 2.4 l-.7 -.5 l-.7 .5z" fill="#2a2a38"/>`; });
  s += '<path d="M-9.6 3.6 Q0 6.6 9.6 3.6 V8.4 Q0 11.4 -9.6 8.4Z" fill="#f2c230"/><path d="M0 6.4 Q4 6.2 9.6 3.6 V8.4 Q4 10.8 0 11Z" fill="#000" opacity=".14"/>';
  [[-6, 7.1, '#4a8ee0'], [0, 8.4, '#d93a3a'], [6, 7.1, '#3fae5a']].forEach(([x, y, c]) => { s += `<circle cx="${x}" cy="${y}" r="1.4" fill="${c}" stroke="#c99a1c" stroke-width=".4"/><circle cx="${x - .4}" cy="${y - .4}" r=".4" fill="#fff"/>`; });
  s += '<circle cx="-3" cy="8" r=".5" fill="#fff"/><circle cx="3" cy="8" r=".5" fill="#fff"/></g>';
  s += `<g transform="translate(-15 22) scale(.62)">${gift('#7a4bd0', '#f2c230')}</g><g transform="translate(0 24) scale(.7)">${gift('#2bb3a3', '#f2c230')}</g><g transform="translate(15 22) scale(.62)">${gift('#d93a3a', '#f2c230')}</g>`;
  return s;
})();

// ------------------------------------------------------------------------------------------------ San Patricio
const lf = '<path d="M0 0 C-6 -3 -5 -9 0 -6 C5 -9 6 -3 0 0Z" fill="#3fae5a"/><path d="M0 0 C6 -3 5 -9 0 -6 Z" fill="#000" opacity=".15"/><path d="M0 -1 V-5.5" stroke="#2a7d40" stroke-width=".7"/><ellipse cx="-2" cy="-5" rx=".8" ry="1.3" fill="#fff" opacity=".3"/>';
const clover = n => { let s = ''; for (let i = 0; i < n; i++) s += `<g transform="rotate(${i * 360 / n})">${lf}</g>`; return `<path d="M0 0 Q2 5 5 10" stroke="#2a7d40" stroke-width="1.4" fill="none"/>${s}<circle r="1" fill="#2a7d40"/>`; };
const coin = '<circle r="7.5" fill="#f2c230"/><circle r="5.6" fill="none" stroke="#c99a1c" stroke-width="1"/><path d="M0 -3.6 L1 -1 L3.7 -.8 L1.7 .9 L2.3 3.5 L0 2.1 L-2.3 3.5 L-1.7 .9 L-3.7 -.8 L-1 -1Z" fill="#c99a1c"/><path d="M-6 -2 A6.5 6.5 0 0 1 -2 -6" stroke="#fff" stroke-width="1" fill="none" opacity=".6"/>';
const hat = '<ellipse cy="6" rx="10.5" ry="3" fill="#1f6a35"/><path d="M-6 6 V-6 Q0 -9 6 -6 V6Z" fill="#2f8f48"/><path d="M0 -7.5 Q6 -6 6 -6 V6 H0Z" fill="#000" opacity=".16"/><ellipse cy="-6" rx="6" ry="2" fill="#37a352"/><rect x="-6" y="1" width="12" height="3.4" fill="#16161c"/><rect x="-2.2" y=".4" width="4.4" height="4.6" fill="none" stroke="#f2c230" stroke-width="1"/><path d="M-5 -4 V3" stroke="#fff" opacity=".2"/>';
const cloud = (x, y) => `<g transform="translate(${x} ${y})"><ellipse rx="5" ry="2.8" fill="#f4f4f7"/><ellipse cx="-3" cy="-1.4" rx="3" ry="2.4" fill="#fff"/><ellipse cx="2" cy="-2" rx="3.4" ry="2.8" fill="#fff"/><ellipse cx="1" cy="1.2" rx="5" ry="1.4" fill="#000" opacity=".08"/></g>`;
const rainbowSmall = (() => { let s = ''; ['#e24b4b', '#f08a24', '#f2c230', '#3fae5a', '#4a8ee0'].forEach((c, i) => { const r = 11 - i * 1.7; s += `<path d="M${-r} 6 A${r} ${r} 0 0 1 ${r} 6" stroke="${c}" stroke-width="1.7" fill="none"/>`; }); return s + cloud(-10, 7) + cloud(10, 7); })();
const mc = (x, y, r) => `<circle cx="${x}" cy="${y}" r="${r}" fill="#f2c230" stroke="#c99a1c" stroke-width=".5"/><circle cx="${x}" cy="${y}" r="${r * .6}" fill="none" stroke="#c99a1c" stroke-width=".4"/><circle cx="${x - r * .35}" cy="${y - r * .35}" r="${r * .18}" fill="#fff" opacity=".7"/>`;
const pHero = (() => {
  let s = ''; ['#e24b4b', '#f08a24', '#f2c230', '#3fae5a', '#4a8ee0', '#8e63d6'].forEach((c, i) => { const r = 18 - i * 2.1; s += `<path d="M${-r} 12 A${r} ${r} 0 0 1 ${r} 12" stroke="${c}" stroke-width="2.2" fill="none"/>`; });
  s += cloud(-18, 13) + cloud(18, 13) + '<ellipse cy="21.5" rx="20" ry="2" fill="#000" opacity=".3"/><rect x="-6.8" y="17.4" width="2.6" height="3" rx=".8" fill="#1a1a22"/><rect x="4.2" y="17.4" width="2.6" height="3" rx=".8" fill="#1a1a22"/><path d="M-9.5 5 Q-12 15 -5.6 18 H5.6 Q12 15 9.5 5Z" fill="#2a2a38"/><path d="M2 5 H9.5 Q12 15 5.6 18 H2Z" fill="#000" opacity=".25"/><path d="M-8 7.4 Q-9 12.4 -6 15.4" stroke="#7a7a8c" stroke-width=".9" fill="none" stroke-linecap="round"/><ellipse cy="5" rx="10.4" ry="2.6" fill="#3a3a48"/><ellipse cy="5.2" rx="8.8" ry="1.9" fill="#f2c230"/>';
  [[-6, 2.2], [0, 1.6], [6, 2.2], [-3, -.4], [3, -.6], [0, -2.6], [-6.6, -0.6]].forEach(([x, y]) => { s += mc(x, y, 2.6); });
  s += '<ellipse cx="9" cy="-5" rx="1.2" ry="2.4" fill="#f2c230" stroke="#c99a1c" stroke-width=".5" transform="rotate(25 9 -5)"/>';
  s += `<g transform="translate(-14 -3)">${stars('#fff', 2, .8)}</g><g transform="translate(13 -9)">${stars('#f7e08a', 1.6, .7)}</g><g transform="translate(5 -9)">${stars('#fff', 1.2, .5)}</g>`;
  s += `<g transform="translate(-19 20) scale(.5)">${clover(3)}</g><g transform="translate(19 20) scale(.5) rotate(20)">${clover(4)}</g><g transform="translate(17 13) scale(.75) rotate(8)">${hat}</g>`;
  return s;
})();

// ------------------------------------------------------------------------------------------------ Pascua
// The egg's bands follow its outline (no clip paths): half-width of the egg at height y.
const EGG = 'M0 -9 C5.5 -9 8 -2 8 3 C8 7.5 4.5 10 0 10 C-4.5 10 -8 7.5 -8 3 C-8 -2 -5.5 -9 0 -9Z';
const ew = y => 7.4 * Math.sqrt(Math.max(0, 1 - Math.pow((y - .5) / 9.5, 2)));
const band = (y1, y2, c) => `<path d="M${f1(-ew(y1))} ${y1} H${f1(ew(y1))} L${f1(ew(y2))} ${y2} H${f1(-ew(y2))}Z" fill="${c}"/>`;
const shine = '<ellipse cx="-3.8" cy="-4" rx="1.2" ry="2.8" fill="#fff" opacity=".5" transform="rotate(15 -3.8 -4)"/><path d="M-6 6.5 Q0 9.5 6 6.5" stroke="#000" stroke-width=".8" fill="none" opacity=".1"/>';
const egg1 = `<path d="${EGG}" fill="#f58aa5"/>${band(-1.6, 1.6, '#fff')}<path d="M-4.6 5 L-2.3 7.2 L0 5 L2.3 7.2 L4.6 5" stroke="#7cc4f0" stroke-width="1.5" fill="none"/><path d="M-5 -5.4 L-2.5 -3.6 L0 -5.4 L2.5 -3.6 L5 -5.4" stroke="#fff" stroke-width="1.2" fill="none"/><circle cx="-4" cy="0" r=".9" fill="#e8496b"/><circle cx="0" cy="0" r=".9" fill="#e8496b"/><circle cx="4" cy="0" r=".9" fill="#e8496b"/><g transform="translate(0 -7.4)">${ringE(5, 1.2, .6, .9, '#fff')}</g>${shine}`;
const egg2 = `<path d="${EGG}" fill="#7cc4f0"/>${band(-6.4, -4.2, '#f7e08a')}${band(5.6, 7.6, '#f58aa5')}<path d="M-5.8 0.4 q1.45 -2.4 2.9 0 t2.9 0 t2.9 0 t2.9 0" stroke="#fff" stroke-width="1.2" fill="none"/>${[-5, -2.5, 0, 2.5, 5].map(x => `<circle cx="${x}" cy="3.2" r=".8" fill="#fff"/>`).join('')}${shine}`;
const egg3 = `<path d="${EGG}" fill="#f7e08a"/>${[[-4, -5, '#e8618c'], [3, -6, '#7cc4f0'], [-1, -1, '#3fae5a'], [5, 0, '#e8618c'], [-5, 2, '#8e63d6'], [1, 4, '#7cc4f0'], [-2.6, 7, '#e8618c'], [3.2, 7.4, '#3fae5a']].map(([x, y, c]) => `<circle cx="${x}" cy="${y}" r="1.3" fill="${c}"/>`).join('')}<path d="M-6.6 2.6 q1.65 -2.2 3.3 0 t3.3 0 t3.3 0 t3.3 0" stroke="#f08a1a" stroke-width=".9" fill="none"/>${shine}`;
const tulip = (c, d) => `<path d="M0 -1 Q.5 6 0 13" stroke="#2f8f48" stroke-width="1.6" fill="none"/><path d="M0 13 Q-9 8 -7 -1 Q-2 3 0 13Z" fill="#3fae5a"/><path d="M0 13 Q8 9 6.5 2 Q2 4 0 13Z" fill="#2f8f48"/><path d="M-5 -2 Q-6 -9 -3 -11.5 Q-2.5 -6 0 -5Z" fill="${d}"/><path d="M5 -2 Q6 -9 3 -11.5 Q2.5 -6 0 -5Z" fill="${d}"/><path d="M-4.6 -2 Q-5 -8 0 -12 Q5 -8 4.6 -2 Q0 1.2 -4.6 -2Z" fill="${c}"/><path d="M-1.2 -3 Q-1.8 -7 0 -9" stroke="#fff" stroke-width="1" fill="none" opacity=".45"/>`;
const chick = '<ellipse cy="-.5" rx="7.2" ry="6.8" fill="#f7d84a"/><ellipse cx="-6.8" cy="0" rx="2" ry="3.4" fill="#ecc72f" transform="rotate(15 -6.8 0)"/><ellipse cx="6.8" cy="0" rx="2" ry="3.4" fill="#ecc72f" transform="rotate(-15 6.8 0)"/><circle cy="-6" r="5.6" fill="#f7d84a"/><path d="M0 -11.6 Q-1.2 -14 .8 -14.4 M1.2 -11.4 Q2.6 -13.4 4 -12.6" stroke="#ecc72f" stroke-width="1.1" fill="none" stroke-linecap="round"/><circle cx="-2.2" cy="-6.8" r="1" fill="#2a2a38"/><circle cx="2.2" cy="-6.8" r="1" fill="#2a2a38"/><circle cx="-2.5" cy="-7.2" r=".35" fill="#fff"/><circle cx="1.9" cy="-7.2" r=".35" fill="#fff"/><path d="M-1.5 -4.9 L1.5 -4.9 L0 -2.8Z" fill="#f08a24"/><ellipse cx="-4" cy="-4.6" rx="1.2" ry=".8" fill="#f5a3a3" opacity=".8"/><ellipse cx="4" cy="-4.6" rx="1.2" ry=".8" fill="#f5a3a3" opacity=".8"/><path d="M-8.5 3 L-6.5 6.5 L-4.2 3.2 L-2 6.5 L0 3.2 L2 6.5 L4.2 3.2 L6.5 6.5 L8.5 3 V10 Q0 12.5 -8.5 10Z" fill="#fffaf0" stroke="#e6dcc6" stroke-width=".5"/>';
const bunny = '<ellipse cx="-3.4" cy="-8.4" rx="2.4" ry="6.6" fill="#f4f4f7" transform="rotate(-8 -3.4 -2)"/><ellipse cx="-3.4" cy="-8.4" rx="1.1" ry="4.6" fill="#f5b5c8" transform="rotate(-8 -3.4 -2)"/><ellipse cx="3.4" cy="-8.4" rx="2.4" ry="6.6" fill="#f4f4f7" transform="rotate(12 3.4 -2)"/><ellipse cx="3.4" cy="-8.4" rx="1.1" ry="4.6" fill="#f5b5c8" transform="rotate(12 3.4 -2)"/><ellipse cy="2" rx="7.8" ry="6.8" fill="#f4f4f7" stroke="#dcdce4" stroke-width=".4"/><ellipse cy="4.4" rx="3.2" ry="2.3" fill="#f6e6ee"/><circle cx="-3" cy="1" r="1" fill="#2a2a38"/><circle cx="3" cy="1" r="1" fill="#2a2a38"/><circle cx="-3.3" cy=".6" r=".35" fill="#fff"/><circle cx="2.7" cy=".6" r=".35" fill="#fff"/><path d="M-1.1 3.4 H1.1 L0 4.7Z" fill="#e8809e"/><path d="M0 4.7 V5.6 M-1.4 6 Q0 6.8 1.4 6" stroke="#b9889a" stroke-width=".5" fill="none"/><path d="M-6 4 L-10.4 3.2 M-6 5.4 L-10.4 6 M6 4 L10.4 3.2 M6 5.4 L10.4 6" stroke="#c9c9d2" stroke-width=".5"/><ellipse cx="-5.2" cy="4.4" rx="1.5" ry="1" fill="#f5b5c8" opacity=".8"/><ellipse cx="5.2" cy="4.4" rx="1.5" ry="1" fill="#f5b5c8" opacity=".8"/><path d="M0 9.4 L-3.6 7.4 V11.4Z M0 9.4 L3.6 7.4 V11.4Z" fill="#e8618c"/><circle cy="9.4" r="1.1" fill="#c92f50"/>';
const carrot = '<path d="M-4.4 -4 Q0 -6.4 4.4 -4 L1.2 11 Q0 12.4 -1.2 11Z" fill="#f08a24"/><path d="M-2.6 -1 H0 M.6 3 H2.6 M-1.6 6.4 H.2" stroke="#d96d10" stroke-width=".8"/><path d="M-3 -4 Q-3.4 0 -2.4 4" stroke="#fbb868" stroke-width=".8" fill="none"/><path d="M0 -4.6 Q-4 -9 -2 -12.4 Q0 -9 0 -4.6Z M0 -4.6 Q0 -10 2 -13 Q3.4 -9 0 -4.6Z M0 -4.6 Q4 -9 6.4 -8.2 Q4.4 -5 0 -4.6Z M0 -4.6 Q-4 -7 -6 -6.4 Q-4 -4.2 0 -4.6Z" fill="#3fae5a"/>';
const daisy = ring(10, 5.2, 2.6, '#fff') + ring(10, 4.6, 2, '#fbe9f0') + '<circle r="2.8" fill="#f2c230"/><circle cx="-.8" cy="-.8" r=".8" fill="#fbe08a"/>';
const basket = '<path d="M-8.4 -2 Q0 -17 8.4 -2" stroke="#a9733a" stroke-width="1.7" fill="none"/><path d="M-9 -2 l.8 -3.4 l1 3.4 l1 -3.8 l1 3.8 l1 -3.2 l1 3.2 l1 -3.6 l1 3.6 l1 -3.4 l1 3.4 l1 -3.8 l1 3.8 l1 -3.4 l1 3.4 l1 -3.2 l1 3.2 l1 -3.6 l1 3.6Z" fill="#4fa94a"/>' + `<g transform="translate(-4.6 -2.4) scale(.55)">${egg1}</g><g transform="translate(0.4 -3.4) scale(.55)">${egg2}</g><g transform="translate(5.2 -2.2) scale(.55)">${egg3}</g>` + '<path d="M-10 -1 H10 L8 10 Q0 12.4 -8 10Z" fill="#c8924d"/><g stroke="#9a6a30" stroke-width=".6" fill="none"><path d="M-9.6 2.4 H9.6 M-9 5.8 H9 M-8.4 8.8 H8.4"/><path d="M-6 -1 V11 M-2 -1 V11.6 M2 -1 V11.6 M6 -1 V11"/></g><rect x="-10.8" y="-2.4" width="21.6" height="2.8" rx="1.2" fill="#a9733a"/><path d="M0 -.6 L-4.4 -3.6 Q-6.2 -.4 0 -.6Z M0 -.6 L4.4 -3.6 Q6.2 -.4 0 -.6Z" fill="#f58aa5"/><circle cy="-1" r="1.2" fill="#e8496b"/>';
const pascuaHero = `${basket}<g transform="translate(18 4) scale(.63)">${tulip('#b99cf0', '#8e63d6')}</g>`;

// ------------------------------------------------------------------------------------------------ the files
// A garland tile: 680 units wide, the string on y 35, the pieces hanging from it. The tile starts and ends at y 35.
function garland(items, extra = '') {
  const W = 680, n = items.length, xs = Array.from({ length: n + 1 }, (_, i) => i * W / n);
  let s = 'M0 35 ';
  for (let i = 1; i <= n; i++) s += `Q${(xs[i - 1] + xs[i]) / 2} 41 ${xs[i]} 35 `;
  let body = `<path d="${s}" stroke="#7a7a85" stroke-width="1.2" fill="none"/>`;
  items.forEach((it, i) => { const x = (xs[i] + xs[i + 1]) / 2; body += `<path d="M${x} 38 L${x} 44" stroke="#7a7a85" stroke-width=".9"/><g transform="translate(${x} 57) scale(1.2)">${it}</g>`; });
  return svg(`0 33 ${W} 58`, body + extra);
}

const F = {
  halloween: {
    garland: garland([pump, bat, ghost, candy, pump, bat, ghost, candy],
      `<path d="M340 38 L340 72" stroke="#9a9aa5" stroke-width=".7"/><g transform="translate(340 80)">${spiderBody}</g>`),
    hero: svg('-25 -28 52 47', hHero),
  },
  muertos: {
    garland: garland([flag('#e8618c'), mari, flag('#f59a1b'), skullMini, flag('#8e63d6'), candleSmall, flag('#2bb3a3'), skullMini]),
    hero: svg('-185 -355 1240 1900', mHero),
  },
  navidad: {
    garland: garland([bulb('#e04848'), ball('#4a8ee0'), cane, bulb('#3fae5a'), bell, bulb('#f2c230'), holly, ball('#d93a3a')]),
    hero: svg('-21 -22 52 42', xmasHero),
  },
  nuevoano: {
    garland: garland([star2('#f2c230', '#f7e08a'), popper, balloon('#f2c230'), clock, confetti, balloon('#cfd3dc')]),
    hero: svg('-28 -31 56 56', nHero),
  },
  reyes: {
    garland: garland([crown, starB, gift('#7a4bd0', '#f2c230'), crown, starB, gift('#2bb3a3', '#f2c230')]),
    hero: svg('-21 -34 42 62', rHero),
  },
  sanvalentin: {
    garland: garland([heartRed, env, arrow, heartPink, rose, heartBow]),
    hero: svg('-22 -24 44 50', vHero),
  },
  sanpatricio: {
    garland: garland([clover(3), coin, hat, clover(3), rainbowSmall, coin]),
    hero: svg('-23 -8 48 31', pHero),
  },
  pascua: {
    garland: garland([egg1, tulip('#e8496b', '#c92f50'), chick, egg2, bunny, carrot, egg3, daisy]),
    hero: svg('-14 -18 40 32', pascuaHero),
  },
};

for (const [key, v] of Object.entries(F)) {
  fs.writeFileSync(path.join(OUT, `${key}-garland.svg`), v.garland);
  fs.writeFileSync(path.join(OUT, `${key}-hero.svg`), v.hero);
}
fs.writeFileSync(path.join(OUT, 'halloween-web.svg'), svg('0 0 100 100', web));
console.log('written to', OUT);
