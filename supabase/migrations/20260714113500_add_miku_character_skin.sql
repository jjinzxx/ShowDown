-- Miku character cosmetic. Runtime id and asset path are registered in
-- UShowDownCharacterSkinCatalog; this migration exposes the matching product.

insert into public.skins (id, name, rarity, price, is_active, type)
values ('miku', 'Miku', 'legendary', 1500, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_sets (id, name, rarity, price, is_active, type)
values ('character_miku_set', 'Miku', 'legendary', 1500, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_set_items (set_id, skin_id, slot)
values ('character_miku_set', 'miku', 'character')
on conflict do nothing;
