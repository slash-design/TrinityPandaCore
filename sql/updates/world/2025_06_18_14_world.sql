-- creature_queststarter
DELETE FROM `creature_queststarter` WHERE `quest` IN (31592, 31591, 31593, 31316, 31966, 31889, 31927, 31902, 31930, 31919, 32008, 31821, 32863);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(63596, 31592),
(63596, 31591),
(63596, 31593),
(63596, 31316),
(63596, 31966),
(63596, 31889),
(63596, 31927),
(63596, 31902),
(63596, 31930),
(63596, 31919),
(63596, 32008),
(63596, 31821),
(63596, 32863);

-- creature_questender
DELETE FROM `creature_questender` WHERE `quest` IN (31592, 31591, 31593, 31316, 32008, 31821, 32863);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(63596, 31592),
(63596, 31591),
(63596, 31593),
(64330, 31316),
(63596, 32008),
(63596, 31821),
(63596, 32863);

-- locales_gossip_menu_option
DELETE FROM `locales_gossip_menu_option` WHERE `menu_id`=14991 AND `id`=1;
INSERT INTO `locales_gossip_menu_option` VALUES
(14991,1,'제 전투 애완동물을 치유하거나 되살리고 싶습니다.','J’aimerais soigner et ressusciter mes mascottes de combat.','Ich würde gern meine Kampfhaustiere heilen und wiederbeleben.','我要治疗和复活我的战斗宠物。','我想要治療並復活我的戰寵。','Me gustaría curar y revivir a mis mascotas de duelo.','Me gustaría curar y revivir a mis mascotas de duelo.','Мне бы хотелось воскресить и исцелить моих боевых питомцев.',NULL,NULL,NULL,'제 전투 애완동물을 치유하거나 되살리고 싶어요.','J’aimerais soigner et ressusciter mes mascottes de combat.','Ich würde gern meine Kampfhaustiere heilen und wiederbeleben.','我要治疗和复活我的战斗宠物。','我想要治療並復活我的戰寵。','Me gustaría curar y revivir a mis mascotas de duelo.','Me gustaría curar y revivir a mis mascotas de duelo.','Мне бы хотелось воскресить и исцелить моих боевых питомцев.',NULL,NULL,NULL,'애완동물을 치유할 보급품을 받으려면 비용을 지불해야 합니다.','Je demande une participation aux frais de matériel.','Es wird eine kleine Gebühr für die medizinische Hilfe erhoben.','为物资支付一些费用是必需的。','購買補給品的小小費用是必須的。','Hay una modesta tarifa para suministros.','Hay una modesta tarifa para suministros.','За это надо бы и заплатить немножко.',NULL,NULL,NULL,'애완동물을 치유할 보급품을 받으려면 비용을 지불해야 합니다.','Je demande une participation aux frais de matériel.','Es wird eine kleine Gebühr für die medizinische Hilfe erhoben.','为物资支付一些费用是必需的。','購買補給品的小小費用是必須的。','Hay una modesta tarifa para suministros.','Hay una modesta tarifa para suministros.','За это надо бы и заплатить немножко.',NULL,NULL,NULL);

-- locales_gossip_menu_option
DELETE FROM `locales_gossip_menu_option` WHERE `menu_id`=14991 AND `id`=0;
DELETE FROM `locales_gossip_menu_option` WHERE `menu_id`=14991 AND `id`=2;
INSERT INTO `locales_gossip_menu_option` (`menu_id`, `id`, `option_text_loc3`, `option_text_female_loc3`) VALUES
(14991, 0, 'Möchtet Ihr einige seltene Begleiter fangen?', 'Möchtet Ihr einige seltene Begleiter fangen?'),
(14991, 2, 'Wollt Ihr Eure Kampfsteine eintauschen?', 'Wollt Ihr Eure Kampfsteine eintauschen?');

-- locales_npc_text
UPDATE `locales_npc_text` SET `Text0_0_loc3`='Was haltet Ihr von meinem Haustier?' WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text0_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text1_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text1_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text2_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text2_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text3_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text3_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text4_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text4_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text5_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text5_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text6_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text6_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text7_0_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;
UPDATE `locales_npc_text` SET `Text7_1_loc3`='Was haltet Ihr von meinem Haustier?'WHERE `ID`=20326;

-- locales_creature
DELETE FROM `locales_creature` WHERE `entry`=63596;
INSERT INTO `locales_creature` VALUES
(63596, 'deDE', 'Marlene Trichdie', '<Kampfhaustiertrainerin>', 18414);