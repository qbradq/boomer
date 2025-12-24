local TestEnt = {}

function TestEnt:think(dt)
    if not self.timer then self.timer = 0 end
    self.timer = self.timer + dt
    
    if self.timer > 1.0 then
        local x,y,z = Entity.GetPos(self.id)
        print("LuaEntity["..self.id.."] Tick. Pos: (" .. 
            string.format("%.1f", x) .. ", " .. 
            string.format("%.1f", y) .. ")")
        self.timer = 0
    end
end

return TestEnt
