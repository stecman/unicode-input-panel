use std::{env, fs::File, collections::HashMap};
use std::io::prelude::*;

#[derive(Copy, Clone)]
struct CodeRange {
    start: u32,
    end: u32,
    index: usize
}

struct FontFace {
    buf: Vec<u8>,
    renderer: fontdue::Font,
    name: String,
}

impl FontFace {
    pub fn load_path(path: &String) -> Result<FontFace, std::io::Error> {
        let mut file = File::open(path)?;
        let mut buf = Vec::new();
        file.read_to_end(&mut buf)?;

        // The fontdue API consumes/moves the passed data even though it doesn't hold onto it
        // We have to copy it here to be able to continue accessing it with ttf_parser later...
        let font = fontdue::Font::from_bytes(buf.to_vec(), fontdue::FontSettings::default()).unwrap();

        // Use filename as the name for debugging
        let name = std::path::Path::new(&path).file_name().unwrap();

        Ok(FontFace {
            buf,
            renderer: font,
            name: name.to_str().unwrap().into(),
        })
    }

    pub fn get_face(&self) -> ttf_parser::Face {
        let face = ttf_parser::Face::from_slice(&self.buf, 0).unwrap();
        return face;
    }
}

fn make_output_path(codepoint: u32) -> (String, String) {
    let mut out_path = "/tmp/fonts".to_owned();
    let mut hex_str = std::format!("{codepoint:04X}");

    // Ensure we have an even length string for codepoints > 2 bytes
    if (hex_str.len() % 2) != 0 {
        hex_str = std::format!("0{hex_str}");
    }

    // Build folder structure to match codepoint bytes
    for (i, c) in hex_str.char_indices() {
        if (i % 2) == 0 {
            out_path.push_str("/");
        }

        out_path.push(c);
    }

    out_path.push_str(".png");

    let (dir,file) = out_path.split_at(out_path.len() - 6);
    return (dir.to_owned(), file.to_owned());
}

fn main() {
    let paths: Vec<String> = env::args().skip(1).collect();

    if paths.len() == 0 {
        panic!("No fonts were passed");
    }

    // Load all font-faces that were passed
    let fonts: Vec<FontFace> = paths.iter().map(|path| {
        //println!("Loading font: {}", path);

        let font = match FontFace::load_path(path) {
            Ok(f) => f,
            Err(error) => panic!("Failed to open font file: {:?}", error),
        };

        return font;
    }).collect();

    // Index available codepoints, pointing to the first font that contains it
    let mut charmap = HashMap::new();

    let mut ranges: Vec<CodeRange> = Vec::new();

    for (index, font) in fonts.iter().enumerate() {
        let face = font.get_face();

        let mut from: u32 = u32::MAX;
        let mut last: u32 = 0;

        if let Some(subtable) = face.tables().cmap {
            for subtable in subtable.subtables {

                subtable.codepoints(|codepoint| {

                    if from == u32::MAX {
                        from = codepoint;

                    } else if (from - codepoint) != 0 {
                        let mut should_insert = true;
                        let mut start = from;
                        let mut end = last;

                        let mut to_remove: Vec<usize> = Vec::new();

                        // Adjust for other ranges if needed
                        for (index, range) in ranges.iter_mut().enumerate() {

                            let their_size = i64::from(range.end - range.start);
                            let our_size: i64 = i64::from(end - start);

                            // Check we haven't been absorbed by existing ranges
                            if our_size <= 0 {
                                should_insert = false;
                                break;
                            }

                            if range.start >= start && range.end <= end {
                                // Existing range is contained in ours: absorb it as we are bigger
                                to_remove.push(index);
                                continue;

                            } else if start > range.end || end < range.start {
                                // No overlap: keep searching
                                continue;

                            } else if start >= range.start && end <= range.end {
                                // Fully contained: existing range wins
                                should_insert = false;
                                break;
                            }

                            if end >= range.start && end <= range.end {
                                // New partially overlaps from the left side
                                if their_size >= our_size {
                                    // Existing range is larger steals part of our range
                                    end = range.start - 1;
                                } else {
                                    // We are larger and steal part of the existing range
                                    range.start = end + 1;
                                }

                            } else if start >= range.start && start <= range.end {
                                // New partially overlaps from the right side
                                if their_size >= our_size {
                                    // Existing range is larger and steals part of our range
                                    start = range.end + 1;
                                } else {
                                    // We are larger and steal part of the existing range
                                    range.start = end + 1;
                                }

                            } else {
                                panic!("Expected to see an overlap between {}-{} and {}-{} but there was none. Hitting this shouldn't be possible...", start, end, range.start, range.end);
                            }
                        }

                        // Drop any empty ranges
                        for index in to_remove.iter().rev() {
                            ranges.remove(*index);
                        }

                        if should_insert {
                            ranges.push(CodeRange{
                                start,
                                end,
                                index,
                            });

                            // Reset sequence
                            from = u32::MAX;

                            // Ensure ranges are still in order by start key
                            ranges.sort_by(|a,b| a.start.cmp(&b.start));
                        }
                    }

                    // Remember the last codepoint to use as the potential end of a range
                    last = codepoint;

                    if !charmap.contains_key(&codepoint) {
                        charmap.insert(codepoint, font);
                    }
                });
            }
        }
    }

    println!("Collected {} codepoints from {} fonts", charmap.len(), paths.len());
    println!("Range count: {}", ranges.len());

    let mut keys: Vec<&u32> = charmap.keys().collect();
    keys.sort();

    let mut compacted: Vec<CodeRange> = Vec::new();
    if ranges.len() > 1 {
        let mut it = ranges.iter();
        let first = it.next().unwrap();

        let mut current: CodeRange = first.clone();

        while let Some(range) = it.next() {
            if current.index != range.index {
                compacted.push(current);
                current = *range;
                continue;
            }

            current.end = range.end;
        }
    }

    for &codepoint in keys.iter() {

        // Create destination path
        let (out_dir, filename) = make_output_path(*codepoint);
        let out_path = std::format!("{out_dir}{filename}");
        std::fs::create_dir_all(out_dir).unwrap();

        const MAX_WIDTH: usize = 240;
        const MAX_HEIGHT: usize = 200;
        let mut px_size = 150;

        // Try to render to bitmap
        {
            let fontface = charmap.get(codepoint).unwrap();
            let face = fontface.get_face();

            // Look for image representations if this is not a control character
            // This allows colour emoji to be drawn, which glyph drawing doesn't handle.
            if let Ok(chr) = (*codepoint).try_into() {
                if let Some(glyph_id) = face.glyph_index(chr) {

                    // Check if this needs to be rendered from an SVG
                    if let Some(svgdata) = face.glyph_svg_image(glyph_id) {
                        let options = usvg::Options::default();
                        if let Ok(_tree) = usvg::Tree::from_data(svgdata, &options.to_ref()) {
                            // TODO: Actually render SVG glyphs
                            println!("Warning: {codepoint} has an SVG representation, but we can't render this yet");
                        }
                    }

                    // Check if this glyph is a stored image
                    if let Some(img) = face.glyph_raster_image(glyph_id, px_size) {
                        if img.format == ttf_parser::RasterImageFormat::PNG {
                            let file = std::fs::File::create(&out_path).unwrap();
                            let ref mut writer = std::io::BufWriter::new(file);

                            if let Err(e) = writer.write_all(img.data) {
                                panic!("Writing embedded PNG data failed: {}", e);
                            }

                            println!("{codepoint} (png) -> {out_path}");
                            continue;
                        }
                    }
                }

                // Brute-force adjust the font size to fit if needed
                // Some characters end up much larger than the canvas otherwise (eg U+012486)
                loop {
                    let metrics = fontface.renderer.metrics(chr, px_size.into());
                    if metrics.width > MAX_WIDTH || metrics.height > MAX_HEIGHT {
                        px_size -= 1;
                        continue;
                    }

                    break;
                }
            }

            // Draw glyph to a bitmap
            let (metrics, bitmap) = fontface.renderer.rasterize(char::from_u32(*codepoint).unwrap(), px_size.into());

            if metrics.width == 0 || metrics.height == 0 {
                println!("Warning: {codepoint} glyph drawing was empty");
                continue;
            }

            // Write bitmap as a PNG
            let file = std::fs::File::create(&out_path).unwrap();
            let ref mut writer = std::io::BufWriter::new(file);
            let mut encoder = png::Encoder::new(
                writer,
                u32::try_from(metrics.width).unwrap(),
                u32::try_from(metrics.height).unwrap()
            );

            encoder.set_color(png::ColorType::Grayscale);
            encoder.set_depth(png::BitDepth::Eight);

            let mut writer = encoder.write_header().unwrap();
            writer.write_image_data(&bitmap).unwrap();

            println!("{codepoint} (draw) -> {out_path}");
        }

    }
}
