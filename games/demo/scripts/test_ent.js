function TestEnt() {
    return {
        think: function(dt) {
            if (!this.timer) this.timer = 0;
            this.timer += dt;
            
            if (this.timer > 1.0) {
                var pos = Entity.GetPos(this.id);
                // Note: print might need checking if it is available, 
                // QuickJS shell has it, but bare engine needs binding?
                // I will add a simple print binding in script_sys.c if needed, 
                // but usually stdio/console is useful.
                // Assuming I need to add print binding or use console.log if I bound it.
                // For now, I'll rely on my Script_Init comment about adding basics.
                // Wait, I didn't add 'print'. 
                // I should assume I need to add 'print' to script_sys.c for this to work.
                if (typeof print !== 'undefined') {
                    print("JSEntity[" + this.id + "] Tick. Pos: (" + 
                        pos.x.toFixed(1) + ", " + 
                        pos.y.toFixed(1) + ")");
                }
                this.timer = 0;
            }
        }
    };
}

TestEnt;
