// ADXL357 evaluation-board mount. All dimensions in mm.
// Pattern source: ADI EngineerZone EVAL-ADXL357Z hole-size answer.
// Confirm the 15 mm square pattern on your board before printing.

base_size = 21;
base_thickness = 3;
hole_spacing = 15;       // Screw centers, X and Y
hole_diameter = 3.2;     // Clearance hole; change for your screws
boss_diameter = 6;       // Reinforcement around each screw
boss_height = 3;         // Board clearance above the base

$fn = 64;
eps = 0.02;

module screw_boss() {
    // Hollow cylinder, extending into the base for a solid union.
    difference() {
        cylinder(d = boss_diameter, h = base_thickness );
        translate([0, 0, -eps])
            cylinder(d = hole_diameter,
                     h = base_thickness + boss_height + 2*eps);
    }
}

module mount() {
    union() {
        difference() {
            translate([-base_size/2, -base_size/2, 0])
                cube([base_size, base_size, base_thickness]);
            for (x = [-hole_spacing/2, hole_spacing/2])
                for (y = [-hole_spacing/2, hole_spacing/2])
                    translate([x, y, -eps])
                        cylinder(d = hole_diameter, h = base_thickness + 2*eps);
        }
        for (x = [-hole_spacing/2, hole_spacing/2])
            for (y = [-hole_spacing/2, hole_spacing/2])
                translate([x, y, 0]) screw_boss();
    }
}

module mag(){
difference(){
mount();
cylinder(2,2.5,2.5,true);
}}


module holder(){
difference(){
difference(){
cube([25,25,3], true);
translate([-10.505,-10.505,0]) cube([21.05,21.05,2]);
}
translate([0,0,-2])
cylinder(1,5,5);
}
}
translate([0,0,1.5])holder();

translate([25,0,0]) mag();



