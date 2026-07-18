-- Retire Micu from sale and add the two new character cosmetics. Micu rows
-- remain intact so existing ownership and equipment references stay valid.

insert into public.skins (id, name, rarity, price, is_active, type)
values
  ('gangman', 'Gangman', 'epic', 1000, true, 'character'),
  ('maskman', 'Maskman', 'rare', 750, true, 'character'),
  ('miku', 'Miku', 'legendary', 1500, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_sets (id, name, rarity, price, is_active, type)
values
  ('character_gangman_set', 'Gangman', 'epic', 1000, true, 'character'),
  ('character_maskman_set', 'Maskman', 'rare', 750, true, 'character'),
  ('character_miku_set', 'Miku', 'legendary', 1500, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_set_items (set_id, skin_id, slot)
values
  ('character_gangman_set', 'gangman', 'character'),
  ('character_maskman_set', 'maskman', 'character'),
  ('character_miku_set', 'miku', 'character')
on conflict do nothing;

update public.skin_sets
set is_active = false
where id = 'character_micu_set';

update public.skins
set is_active = false
where id = 'micu';
