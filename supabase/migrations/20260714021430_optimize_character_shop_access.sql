-- Keep authenticated shop reads efficient as the player tables grow.
-- The indexes cover reverse foreign-key lookups reported by the advisor.
-- The filename version matches the migration recorded by the hosted project.
create index if not exists player_equipment_equipped_skin_id_idx
on public.player_equipment (equipped_skin_id);

create index if not exists player_skins_skin_id_idx
on public.player_skins (skin_id);

-- Wrapping auth.uid() in a scalar subquery lets Postgres evaluate it once per
-- statement instead of once per candidate row while preserving ownership.
alter policy "Users can read own wallet"
on public.player_wallets
using ((select auth.uid()) = user_id);

alter policy "Users can read own skins"
on public.player_skins
using ((select auth.uid()) = user_id);

alter policy "Users can insert own skins"
on public.player_skins
with check ((select auth.uid()) = user_id);

alter policy "Users can read own skin sets"
on public.player_skin_sets
using ((select auth.uid()) = user_id);

alter policy "Users can read own equipment"
on public.player_equipment
using ((select auth.uid()) = user_id);

alter policy "Users can insert own equipment"
on public.player_equipment
with check ((select auth.uid()) = user_id);

alter policy "Users can update own equipment"
on public.player_equipment
using ((select auth.uid()) = user_id)
with check ((select auth.uid()) = user_id);
