$fn = 90;

// Panel dimensions (adjust as needed)
a = 20;
h = 50;
l = 205;
id = 34;
wall = 2.2;

screw_radius = (2.95/2);
hole_offset_w = 7.75 + screw_radius;

hole_offset_top = 5.5 + screw_radius;
hole_offset_bot = 5 + screw_radius;

module enclosure(x,y,z,a=20) {
	difference() {
		// Box
		rotate([-a, 0, 0]) cube([x, y, z]);
		
		// Flatten bottom
		translate([-x/2, -y/2, -z]) cube([x*2, y*2, z]);
	}
}

// Enclosure
union() {
	// Main box shell
	difference() {
		enclosure(l, id, h);
		translate([wall, -wall, wall]) enclosure(l - wall*2, id, h - (wall*2));
	}
	
	// Screw posts
	for (x = [5, l - 5]) {
		for (y = [5, h - 5]) {
			intersection() {
				// Cut any extensions outside of the panel area
				enclosure(l, id, h);
				rotate([90-20, 0, 0]) translate([x, y, -6]) cylinder(h=id, d=20);
			}
		}
	}

	// Middle rib
	difference() {
		intersection() {
			// Cut any extensions outside of the panel area
			enclosure(l, id, h);
			translate([l/2 -5 , 0, 0]) cube([10, 100, h]);
		}
		
		translate([0, -wall, wall*2]) enclosure(l, id-wall, h - wall*4);
	}
}