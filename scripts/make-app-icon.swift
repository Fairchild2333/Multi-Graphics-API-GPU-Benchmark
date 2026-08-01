// Builds macOS app-icon masters from an edge-to-edge square artwork.
// Uses Apple's fixed grid (824 body in a 1024 canvas, 22.37% corner radius)
// rather than detecting the source's own radius, which is unreliable when the
// artwork's background and the area outside it are both near-white.
import AppKit

let src = CommandLine.arguments[1]
let outLight = CommandLine.arguments[2]
let outDark = CommandLine.arguments.count > 3 ? CommandLine.arguments[3] : nil

guard let img = NSImage(contentsOfFile: src),
      let cg = img.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
    fatalError("cannot load \(src)")
}
let cs = CGColorSpaceCreateDeviceRGB()
let canvas: CGFloat = 1024, body: CGFloat = 824
let inset = (canvas - body) / 2
let radius = body * 0.2237          // Apple's macOS squircle ratio

func render(dark: Bool, to path: String) {
    guard let ctx = CGContext(data: nil, width: Int(canvas), height: Int(canvas),
                              bitsPerComponent: 8, bytesPerRow: 0, space: cs,
                              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
    else { fatalError("ctx") }
    ctx.clear(CGRect(x: 0, y: 0, width: canvas, height: canvas))
    ctx.interpolationQuality = .high
    let rect = CGRect(x: inset, y: inset, width: body, height: body)
    ctx.addPath(CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius,
                       transform: nil))
    ctx.clip()

    if dark {
        // Repaint the desaturated light plate dark, leaving the saturated blue
        // chip untouched. Only valid because this artwork's background is
        // near-neutral and its subject is strongly saturated.
        let w = cg.width, h = cg.height
        var px = [UInt8](repeating: 0, count: w*h*4)
        guard let rd = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8,
                                 bytesPerRow: w*4, space: cs,
                                 bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        else { fatalError("rd") }
        rd.draw(cg, in: CGRect(x: 0, y: 0, width: w, height: h))
        for i in stride(from: 0, to: px.count, by: 4) {
            let r = Double(px[i]), g = Double(px[i+1]), b = Double(px[i+2])
            let mx = max(r, max(g, b)), mn = min(r, min(g, b))
            let sat = mx > 0 ? (mx - mn) / mx : 0
            if sat < 0.18 && mx > 140 {            // neutral + light == the plate
                let t = (mx - 140) / 115           // keep the plate's own shading
                let v = 32.0 + t * 18.0
                px[i] = UInt8(v); px[i+1] = UInt8(v); px[i+2] = UInt8(v + 4)
            }
        }
        if let recol = rd.makeImage() { ctx.draw(recol, in: rect) }
    } else {
        ctx.draw(cg, in: rect)
    }

    guard let out = ctx.makeImage() else { fatalError("makeImage") }
    let rep = NSBitmapImageRep(cgImage: out)
    try! rep.representation(using: .png, properties: [:])!
        .write(to: URL(fileURLWithPath: path))
    print("wrote \(path)")
}

render(dark: false, to: outLight)
if let d = outDark { render(dark: true, to: d) }
