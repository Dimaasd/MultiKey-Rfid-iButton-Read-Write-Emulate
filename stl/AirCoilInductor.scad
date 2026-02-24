//this is to build a straight open air coil inductor.

/* [Size] */

diameter = 20;
length = 2.0;
thickness = 3;
segments = 100;

/* [Lip] */

lip_width = 0.8;
lip_offset = 6.5;

/* [Holes] */

hole_upper = "no"; //[yes, no]
hole_lower = "yes"; //[yes, no]
hole_diameter = 0.5;

/* [Hidden] */

$fn = segments;

//build
difference() {
	OuterCoil();
    InnerCoil();
    
    if(hole_upper == "yes") {
        Hole([0, 0, lip_width + length+1 ]);
    }
    
    if(hole_lower == "yes") {
        Hole([0, 0, lip_width+0.3]);
    }
}

//modules
module OuterCoil() {
	union() {
		//base lip
		cylinder (
			h = lip_width, 
			d = lip_offset * 2 + diameter,
			center = false);

		translate ([0, 0, lip_width]) {
			//coil holder
			cylinder (
				h = length, 
				d = diameter,
				center = false);

			translate ([0, 0, length]) {
				//top lip
				cylinder (
					h = lip_width, 
					d = lip_offset * 2 + diameter,
					center = false);
			}	
		}
	}
}

module InnerCoil() {
    translate ([0, 0, -1]) {
        cylinder (
        h = lip_width * 2 + length + 2, 
        d = diameter - thickness * 2,
        center = false);
    }
}

module Hole(pos=[0, 0, 0]) {
    translate(pos) {
        rotate([0, 90, 0]) {
            cylinder (
                h = lip_width * 2 + diameter, 
                d = hole_diameter,
                center = false);
        }
    }
}